#include <stdio.h>
#include <stdlib.h>
#include "Game.h"
#include "Board.h"
#include "MainAux.h"
#include "Game.h"
#include "FileManager.h"
#include "ValidBoard.h"
#define DIMENSION 9
#define ROWPERBLOCK 3
#define COLPERBLOCK 3
#include "Solve.h"
#include "Stack.h"
//#include "History.h"

void print_me(int **arr,int dimension)
{
    int index_row,index_col;
    for(index_row=0; index_row<dimension; index_row++){
        for(index_col=0; index_col<dimension; index_col++)
        {
            printf("%d ",arr[index_row][index_col]);
        }
        printf("\n");
    }
}

int main() {
Board *game=load("C:\\Users\\LENOVO\\Documents\\New folder\\type");
print_board(game->arr,game->fixed,game->error,game->dimension,game->row_per_block,game->col_per_block);

printf("%d",deter_solve(game->arr,game->dimension,game->row_per_block,game->col_per_block));
print_board(game->arr,game->fixed,game->error,game->dimension,game->row_per_block,game->col_per_block);
    return 0;
}
