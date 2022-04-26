#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string.h>
#include <chrono>
#include <random>
#include <algorithm>

#include "CBoard.h"
#include "ai.h"
#include "parallel_for.h"

// Experiment script

const int n = 6;

void match(
    int player,
    double strength, 
    bool isPlayingWhite, 
    int (&arr)[n][3],
    bool againsStockFish)
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator (seed);
    std::uniform_real_distribution<double> distribution(0.0,1.0);
    CBoard board;
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
            best_move = gm.find_best_or_worst_move(againsStockFish);
        }
        if (!best_move.Valid())
        {
            // Oops. No legal move was found
            bool check = board.isOtherKingInCheck();
            if (check) arr[player][board.whiteToMove() == isPlayingWhite ? 0 : 2]++;
            else arr[player][1]++;
            break;
        }
        board.make_move(best_move);

        // 50-move rule
        if(board.fiftyMoveDraw())
        {
            //std::cerr << "Evoke draw by 50-move rule\n";
            arr[player][1]++;
            break;
        }
    }
}

int main()
{
    freopen("result.csv", "w", stdout);
    int n_games = 20, StinkFish[n][3], StockFish[n][3];
    memset(StinkFish, 0, sizeof(StinkFish));
    memset(StockFish, 0, sizeof(StockFish));
    
    pl::async_par_for(0, n, [&](int i)
    {
        pl::async_par_for(0, n_games, [&](int j)
        {
            std::cerr << "Against StinkFish " << i << ' ' << j << '\n';
            match(i, (double) i/(n-1), j&1, StinkFish, false);
        });

        pl::async_par_for(0, n_games, [&](int j)
        {
            std::cerr << "Against StockFish " << i << ' ' << j << '\n';
            match(i, (double) i/(n-1), j&1, StockFish, true);
        });
    });

    // for (int i = 0; i < n; i++)
    // {
    //     double strength = (double) i/(n-1);
    //     for (int j = 0; j < n_games; j++)
    //     {
    //         std::cerr << "Against StinkFish " << i << ' ' << j << '\n';
    //         match(i, strength, j&1, StinkFish, false, seed);
    //     }
    //     for (int j = 0; j < n_games; j++)
    //     {
    //         std::cerr << "Against StockFish " << i << ' ' << j << '\n';
    //         match(i, strength, j&1, StockFish, true, seed);
    //     }
    // }

    for (int i = 0; i < n; i++)
    {
        printf("%d,%d,%d,%d,%d,%d\n", StinkFish[i][0], StinkFish[i][1], StinkFish[i][2], StockFish[i][0], StockFish[i][1], StockFish[i][2]);
    }
}