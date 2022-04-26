#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string.h>
#include <chrono>
#include <random>

#include "CBoard.h"
#include "CPerft.h"
#include "CPerftSuite.h"
#include "CSearchSuite.h"
#include "ai.h"
#include "trace.h"

#ifdef ENABLE_TRACE
std::ostream *gpTrace = 0;
#endif

// Experiment script



int main()
{
    freopen("result.csv", "w", stdout);
    int n = 11, n_games = 1, StinkFish[n][3], StockFish[n][3];
    
    unsigned seed = 2022;//std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator (seed);
    std::uniform_real_distribution<double> distribution(0.0,1.0);
    for (int i = 0; i < n; i++)
    {
        double strength = (double) i/(n-1);
        bool isPlayingWhite = true;
        for (int j = 0; j < n_games; j++)
        {
            std::cerr << "Against StinkFish " << i << ' ' << j << '\n';
            CBoard board = CBoard();
            AI gm = AI(board), levy = AI(board);
            while (true)
            {
                CMove best_move;
                if (isPlayingWhite && board.whiteToMove())
                {
                    best_move = levy.find_best_or_worst_move(distribution(generator) <= strength);
                }
                else
                {
                    best_move = gm.find_best_or_worst_move(false);
                }
                if (!best_move.Valid())
                {
                    // Oops. No legal move was found
                    bool check = board.isOtherKingInCheck();
                    if (check) StinkFish[i][board.whiteToMove() == isPlayingWhite ? 0 : 2]++;
                    else StinkFish[i][1]++;
                    break;
                }
                board.make_move(best_move);
            }

            isPlayingWhite = !isPlayingWhite;
        }

        isPlayingWhite = true;
        for (int j = 0; j < n_games; j++)
        {
            std::cerr << "Against StockFish " << i << ' ' << j << '\n';
            CBoard board = CBoard();
            AI gm = AI(board), levy = AI(board);
            while (true)
            {
                CMove best_move;
                if (isPlayingWhite && board.whiteToMove())
                {
                    best_move = levy.find_best_or_worst_move(distribution(generator) <= strength);
                }
                else
                {
                    best_move = gm.find_best_or_worst_move(true);
                }
                if (!best_move.Valid())
                {
                    // Oops. No legal move was found
                    bool check = board.isOtherKingInCheck();
                    if (check) StockFish[i][board.whiteToMove() == isPlayingWhite ? 0 : 2]++;
                    else StockFish[i][1]++;
                    break;
                }
                board.make_move(best_move);
            }

            isPlayingWhite = !isPlayingWhite;
        }
    }

    for (int i = 0; i < n; i++)
    {
        printf("%d,%d,%d,%d,%d,%d\n", StinkFish[i][0], StinkFish[i][1], StinkFish[i][2], StockFish[i][0], StockFish[i][1], StockFish[i][2]);
    }
}