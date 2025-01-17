#include <iostream>
#include <stdlib.h>
#include <time.h>

#include "ai.h"
#include "CTime.h"

const int INFTY = 9999;

/***************************************************************
 * This is an implementation of
 * "NegaMax with Alpha Beta Pruning and Transposition Tables"
 * as seen on http://en.wikipedia.org/wiki/Negamax
 * 
 * See also http://frayn.net/beowulf/theory.html
 * and http://fierz.ch/strategy.htm
 *
 * Here alpha is a lower bound and beta is an upper bound,
 * both of which are non-inclusive.
 * In other words: Assume the result lies in the open interval
 * ]alpha, beta[
 * If the returned value is in this range, then it is exact.
 * If the returned value is beta or higher, then it is a new lower bound.
 * If the returned value is alpha or lower, then it is a new upper bound.
 *
 * The value returned is the value of the side to move.
 *
 ***************************************************************/
int AI::search(int alpha, int beta, int level, CMoveList& pv)
{
    // Check for illegal position (side NOT to move is in check).
    // In other words, the side to move can capture the opponents position.
    // This is an illegal position but corresponds to an immediate win.
    // Return a large positive value.
    if (m_board.isOtherKingInCheck()) return 9000 + level;

    // First we check if we are at leaf of tree.
    // If so, return value from NNUE.
    if (level == 0)
    {
        int val = m_board.getValue();

        // If a capture sequence was found, store the first move in the hash table.
        // This is an optimization that improves move ordering.
        if (pv.size())
        {
            CHashEntry hashEntry;
            if (val <= alpha)
            {
                hashEntry.m_nodeTypeAndLevel.nodeType = nodeUpper;
            }
            else if (val >= beta)
            {
                hashEntry.m_nodeTypeAndLevel.nodeType = nodeLower;
            }
            else
            {
                hashEntry.m_nodeTypeAndLevel.nodeType = nodeExact;
            }

            hashEntry.m_nodeTypeAndLevel.level = level;
            hashEntry.m_hashValue              = m_hashEntry.m_hashValue;
            hashEntry.m_searchValue            = val;
            hashEntry.m_bestMove               = pv[0];

            m_hashTable.insert(hashEntry);

            m_killerMove = pv[0];
        }

        return val;
    }

    m_nodes++;

    // Next, check if we have been at this position before (possibly with 
    // shallower search). This works extremely well together with iterative
    // deepening. Another possibility for position arising again is due to
    // transpositions, but that is less frequent.
    // The main benefit is that it gives a good move to search first.
    CHashEntry hashEntry;
    if (m_hashTable.find(m_hashEntry.m_hashValue, hashEntry))
    {

        // Now we examine the search value stored.
        // This value is only to be trusted, if the
        // search level is sufficiently high.
        // Even though the level is not sufficient,
        // we may still use the stored move. This is done later.
        if (hashEntry.m_nodeTypeAndLevel.level >= level)
        {
            switch (hashEntry.m_nodeTypeAndLevel.nodeType)
            {
                case nodeLower :
                    // Lower bound. The true value may be more.
                    if (hashEntry.m_searchValue >= alpha)
                        alpha = hashEntry.m_searchValue;
                    break;

                case nodeUpper :
                    // Upper bound. The true value may be less.
                    if (hashEntry.m_searchValue <= beta)
                        beta = hashEntry.m_searchValue;
                    break;

                default : // case nodeExact :
                    // Exact value. We are done!
                    return hashEntry.m_searchValue;
            }

            // Check if window is closed.
            if (alpha >= beta)
            {
                return hashEntry.m_searchValue;
            }
        } // end of if level
    } // end of m_hashTable.find

    // Prepare to search through all legal moves.
    CMoveList moves;
    m_board.find_legal_moves(moves);

    // If we have been at this position before, which move was the best?
    // Search this move first, because it is likely to still be the best.
    // This often provides a quick refutation of the previous move, 
    // and therefore saves a lot of time.
    if (hashEntry.m_bestMove.Valid())
    {
        for (unsigned int i=0; i<moves.size(); ++i)
        {
            if (moves[i] == hashEntry.m_bestMove)
            {
                moves[i] = moves[0];
                moves[0] = hashEntry.m_bestMove;
                break;
            }
        }
    }
    else
    { // Search captures first.
        unsigned int j=0;
        for (unsigned int i=0; i<moves.size(); ++i)
        {
            if (moves[i].is_it_a_capture())
            {
                CMove tmpMove = moves[i];
                moves[i] = moves[j];
                moves[j] = tmpMove;
                j++;
            }
        }
    }

    int best_val = -INFTY;
    int alpha_orig = alpha;

    // Loop through all legal moves.
    for (unsigned int i=0; i<moves.size(); ++i)
    {
        CMove move = moves[i];

#ifdef DEBUG_HASH
        uint32_t oldHash = m_board.calcHash();
        CHashEntry hashCopy(m_hashEntry);
#endif

        // Do a recursive search
        m_moveList.push_back(move);
        m_hashEntry.update(m_board, move);
        m_board.make_move(move);

        CMoveList pv_temp;
        int val = -search(-beta, -alpha, level-1, pv_temp);

        m_board.undo_move(move);
        m_hashEntry.update(m_board, move);
        m_moveList.pop_back();

#ifdef DEBUG_HASH
        uint32_t newHash = m_board.calcHash();
        if (oldHash != newHash) exit(-1);
        if (hashCopy != m_hashEntry) exit(-1);
#endif

        if (val > best_val)
        {
            // This is the best move so far.
            best_val = val;

            pv = move;
            pv += pv_temp;
        }

        // Now comes the part specific for alpha-beta pruning:
        // Since we are only interested, if another
        // move is better, we update our lower bound.
        if (val > alpha)
        {
            alpha = val;
        }
        // Now we check if the window has been closed.
        // If so, then stop the search.
        if (alpha >= beta)
        {
            // This is fail-soft, since we are returning the value best_val,
            // which might be outside the window.
            break;
        }

        if (m_pvSearch)
        {
            CTime now;
            if (m_timeEnd < now) return alpha;
        }

    } // end of for

    // If our king was captured, check for stalemate
    if (best_val < -8000)
    {
        if (!m_board.isKingInCheck())
            best_val = 0;
    }

    // Finally, store the result in the hash table.
    // We must be careful to determine whether the value is 
    // exact or a bound.
    if (best_val <= alpha_orig)
    {
        hashEntry.m_nodeTypeAndLevel.nodeType = nodeUpper;
    }
    else if (best_val >= beta)
    {
        hashEntry.m_nodeTypeAndLevel.nodeType = nodeLower;
    }
    else
    {
        hashEntry.m_nodeTypeAndLevel.nodeType = nodeExact;
    }

    hashEntry.m_nodeTypeAndLevel.level = level;
    hashEntry.m_hashValue              = m_hashEntry.m_hashValue;
    hashEntry.m_searchValue            = best_val;
    hashEntry.m_bestMove               = pv[0];

    m_hashTable.insert(hashEntry);

    return best_val;
} // end of int search

int AI::search_reverse(int alpha, int beta, int level, CMoveList& pv)
{
    // Check for illegal position (side NOT to move is in check).
    // In other words, the side to move can capture the opponents position.
    // This is an illegal position but corresponds to an immediate win.
    // Return a large positive value.
    if (m_board.isOtherKingInCheck()) return -9000 - level;

    // First we check if we are at leaf of tree.
    // If so, return value from NNUE.
    if (level == 0)
    {
        int val = m_board.getValue();

        // If a capture sequence was found, store the first move in the hash table.
        // This is an optimization that improves move ordering.
        if (pv.size())
        {
            CHashEntry hashEntry;
            if (val <= beta)
            {
                hashEntry.m_nodeTypeAndLevel.nodeType = nodeUpper;
            }
            else if (val >= alpha)
            {
                hashEntry.m_nodeTypeAndLevel.nodeType = nodeLower;
            }
            else
            {
                hashEntry.m_nodeTypeAndLevel.nodeType = nodeExact;
            }

            hashEntry.m_nodeTypeAndLevel.level = level;
            hashEntry.m_hashValue              = m_hashEntry.m_hashValue;
            hashEntry.m_searchValue            = val;
            hashEntry.m_bestMove               = pv[0];

            m_hashTable.insert(hashEntry);

            m_killerMove = pv[0];
        }

        return val;
    }

    m_nodes++;

    // Next, check if we have been at this position before (possibly with 
    // shallower search). This works extremely well together with iterative
    // deepening. Another possibility for position arising again is due to
    // transpositions, but that is less frequent.
    // The main benefit is that it gives a good move to search first.
    CHashEntry hashEntry;
    if (m_hashTable.find(m_hashEntry.m_hashValue, hashEntry))
    {
        // Now we examine the search value stored.
        // This value is only to be trusted, if the
        // search level is sufficiently high.
        // Even though the level is not sufficient,
        // we may still use the stored move. This is done later.
        if (hashEntry.m_nodeTypeAndLevel.level >= level)
        {
            switch (hashEntry.m_nodeTypeAndLevel.nodeType)
            {
                case nodeLower :
                    // Lower bound. The true value may be more.
                    if (hashEntry.m_searchValue <= alpha)
                        alpha = hashEntry.m_searchValue;
                    break;

                case nodeUpper :
                    // Upper bound. The true value may be less.
                    if (hashEntry.m_searchValue >= beta)
                        beta = hashEntry.m_searchValue;
                    break;

                default : // case nodeExact :
                    // Exact value. We are done!
                    return hashEntry.m_searchValue;
            }

            // Check if window is closed.
            if (alpha <= beta)
            {
                return hashEntry.m_searchValue;
            }
        } // end of if level
    } // end of m_hashTable.find

    // Prepare to search through all legal moves.
    CMoveList moves;
    m_board.find_legal_moves(moves);

    // If we have been at this position before, which move was the best?
    // Search this move first, because it is likely to still be the best.
    // This often provides a quick refutation of the previous move, 
    // and therefore saves a lot of time.
    if (hashEntry.m_bestMove.Valid())
    {
        for (unsigned int i=0; i<moves.size(); ++i)
        {
            if (moves[i] == hashEntry.m_bestMove)
            {
                moves[i] = moves[0];
                moves[0] = hashEntry.m_bestMove;
                break;
            }
        }
    }
    else
    { // Search captures first.
        unsigned int j=0;
        for (unsigned int i=0; i<moves.size(); ++i)
        {
            if (moves[i].is_it_a_capture())
            {
                CMove tmpMove = moves[i];
                moves[i] = moves[j];
                moves[j] = tmpMove;
                j++;
            }
        }
    }

    int worst_val = INFTY;
    int beta_orig = beta;

    // Loop through all legal moves.
    for (unsigned int i=0; i<moves.size(); ++i)
    {
        CMove move = moves[i];

#ifdef DEBUG_HASH
        uint32_t oldHash = m_board.calcHash();
        CHashEntry hashCopy(m_hashEntry);
#endif

        // Do a recursive search
        m_moveList.push_back(move);
        m_hashEntry.update(m_board, move);
        m_board.make_move(move);

        CMoveList pv_temp;
        int val = -search_reverse(-beta, -alpha, level-1, pv_temp);

        m_board.undo_move(move);
        m_hashEntry.update(m_board, move);
        m_moveList.pop_back();

#ifdef DEBUG_HASH
        uint32_t newHash = m_board.calcHash();
        if (oldHash != newHash) exit(-1);
        if (hashCopy != m_hashEntry) exit(-1);
#endif

        if (val < worst_val)
        {
            // This is the worst move so far.
            worst_val = val;

            pv = move;
            pv += pv_temp;
        }

        // Now comes the part specific for alpha-beta pruning:
        // Since we are only interested, if another
        // move is better, we update our lower bound.
        if (val < alpha)
        {
            alpha = val;
        }
        // Now we check if the window has been closed.
        // If so, then stop the search.
        if (alpha <= beta)
        {
            // This is fail-soft, since we are returning the value worst_val,
            // which might be outside the window.
            break;
        }

        if (m_pvSearch)
        {
            CTime now;
            if (m_timeEnd < now) return alpha;
        }

    } // end of for

    // If our king was captured, check for stalemate
    if (worst_val < -8000)
    {
        if (!m_board.isKingInCheck())
            worst_val = 0;
    }

    // Finally, store the result in the hash table.
    // We must be careful to determine whether the value is 
    // exact or a bound.
    if (worst_val >= beta_orig)
    {
        hashEntry.m_nodeTypeAndLevel.nodeType = nodeLower;
    }
    else if (worst_val <= alpha)
    {
        hashEntry.m_nodeTypeAndLevel.nodeType = nodeUpper;
    }
    else
    {
        hashEntry.m_nodeTypeAndLevel.nodeType = nodeExact;
    }

    hashEntry.m_nodeTypeAndLevel.level = level;
    hashEntry.m_hashValue              = m_hashEntry.m_hashValue;
    hashEntry.m_searchValue            = worst_val;
    hashEntry.m_bestMove               = pv[0];

    m_hashTable.insert(hashEntry);

    return worst_val;
} // end of int search

/***************************************************************
 * find_best_or_worst_move
 *
 * This is the main AI.
 * It returns what it considers to be the best (or worst) legal 
 * move in the current position.
 ***************************************************************/
CMove AI::find_best_or_worst_move(bool bestMove)
{
    m_nodes = 0;
    m_hashEntry.set(m_board);
    m_moveList.clear();

    CTime timeStart;
    m_timeEnd = timeStart;
    m_timeEnd += 20000; // 20 seconds
    CMoveList moves;
    m_board.find_legal_moves(moves);

    CMoveList best_moves;

    CMoveList pv;
    int num_good;

    if(bestMove)
    {
        int best_val, level = 0;
        while (true)
        {
            CMove best_move;

            best_moves.clear();
            best_val = -INFTY;
            num_good = 0;

            m_pvSearch = true;

            for (unsigned int i=0; i<moves.size(); ++i)
            {
                // We are looking for values in the range [best_val, INFTY[, 
                // which is the same as ]best_val-1, INFTY[
                int alpha = best_val-1;
                int beta = INFTY;

                CMove move = moves[i];

                m_moveList.push_back(move);
                m_hashEntry.update(m_board, move);
                m_board.make_move(move);

                CMoveList pv_temp;
                int val = -search(-beta, -alpha, level, pv_temp);

                m_board.undo_move(move);
                m_hashEntry.update(m_board, move);
                m_moveList.pop_back();

                if (val > best_val)
                {
                    num_good = 0;
                }

                if (val >= best_val)
                {
                    num_good++;

                    pv = move;
                    pv += pv_temp;

                    best_val = val;
                    best_move = move;

                    CTimeDiff timeDiff(timeStart);

                    // unsigned long millisecs = timeDiff.millisecs();

                    // unsigned long nps = 0;
                    // if (millisecs)
                    //     nps = (m_nodes*1000)/millisecs;

                    // std::cout << "info depth " << level << " score cp " << best_val;
                    // std::cout << " time " << millisecs << " nodes " << m_nodes << " nps " << nps;
                    // std::cout << " pv " << pv << std::endl;

                    // This is the move reordering. Good moves are searched first on next iteration.
                    best_moves.insert_front(move);
                }
                else
                {
                    // This is the move reordering. Bad moves are searched last on next iteration.
                    best_moves.push_back(move);
                }

                m_pvSearch = false;
                
                CTime now;
                if (m_timeEnd < now) break;
            } // end of for

            moves = best_moves;

            CTimeDiff timeDiff(timeStart);

            // unsigned long millisecs = timeDiff.millisecs();

            // unsigned long nps = 0;
            // if (millisecs)
            //     nps = (m_nodes*1000)/millisecs;

            // std::cout << "info depth " << level << " score cp " << best_val;
            // std::cout << " time " << millisecs << " nodes " << m_nodes << " nps " << nps;
            // std::cout << " pv " << pv << std::endl;

            CTime now;
            if (m_timeEnd < now) break;
            level += 2;
        }
    }
    else
    {
        // std::cout << "ATTEMP TO FIND WORST MOVE\n";
        int worst_val, level = 0;
        while (true)
        {
            CMove best_move;

            best_moves.clear();
            worst_val = INFTY;
            num_good = 0;

            m_pvSearch = true;
            // std::cerr << "There are " << moves.size() << "legal moves\n";
            for (unsigned int i=0; i<moves.size(); ++i)
            {
                // std::cerr << "iterate: " << i << ' ' << moves[i].ToLongString() << '\n';
                // We are looking for values in the range [-INFTY, worst_val[, 
                // which is the same as [-INFTY, worst_val+1[
                int alpha = worst_val+1;
                int beta = -INFTY;

                CMove move = moves[i];

                m_moveList.push_back(move);
                m_hashEntry.update(m_board, move);
                m_board.make_move(move);

                // std::cerr << m_board << '\n';

                CMoveList pv_temp;
                int val = -search_reverse(-beta, -alpha, level, pv_temp);

                m_board.undo_move(move);
                m_hashEntry.update(m_board, move);
                m_moveList.pop_back();
                
                // std::cerr << m_board << '\n';

                if (val < worst_val)
                {
                    num_good = 0;
                }

                if (val <= worst_val)
                {
                    num_good++;

                    pv = move;
                    pv += pv_temp;

                    worst_val = val;
                    best_move = move;

                    CTimeDiff timeDiff(timeStart);

                    // unsigned long millisecs = timeDiff.millisecs();

                    // unsigned long nps = 0;
                    // if (millisecs)
                    //     nps = (m_nodes*1000)/millisecs;

                    // std::cerr << "info depth " << level << " score cp " << worst_val;
                    // std::cerr << " time " << millisecs << " nodes " << m_nodes << " nps " << nps;
                    // std::cerr << " pv " << pv << std::endl;

                    // This is the move reordering. Good moves are searched first on next iteration.
                    best_moves.insert_front(move);
                }
                else
                {
                    // This is the move reordering. Bad moves are searched last on next iteration.
                    best_moves.push_back(move);
                }

                m_pvSearch = false;

                CTime now;
                if (m_timeEnd < now) break;
            } // end of for

            moves = best_moves;

            CTimeDiff timeDiff(timeStart);

            // unsigned long millisecs = timeDiff.millisecs();

            // unsigned long nps = 0;
            // if (millisecs)
            //     nps = (m_nodes*1000)/millisecs;

            // std::cerr << "info depth " << level << " score cp " << worst_val;
            // std::cerr << " time " << millisecs << " nodes " << m_nodes << " nps " << nps;
            // std::cerr << " pv " << pv << std::endl;

            CTime now;
            if (m_timeEnd < now) break;
            level += 2;
        }
    }

    CMove move;
    if(num_good) move = best_moves[rng()%num_good];
    return move;
} // end of CMove find_best_or_worst_move(CBoard &board)
