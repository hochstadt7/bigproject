/* validation of value in row */
#include "ValidBoard.h"
#include "Board.h"

int in_row(const int *arr, int dimension, int value) {
    int index;
    for (index = 0; index < dimension; index++) {
        if (arr[index] == value)
            return 1;
    }
    return 0;
}
/* validation of value in column */
int in_col(int **arr, int dimension, int col, int value) {
    int index;
    for (index = 0; index < dimension; index++) {
        if (arr[index][col] == value)
            return 1;
    }

    return 0;
}
/* validation of value in block */
int in_block(int **arr, int block_start_row, int block_start_col, int value, int row_per_block,
             int col_per_block) {
    int row, col;
    for (row = 0; row < row_per_block; row++) {
        for (col = 0; col < col_per_block; col++) {
            if (arr[row + block_start_row][col + block_start_col] == value)
            {
                return 1;
            }
        }
    }
    return 0;
}
/*check if requested assignment is legal*/
int is_valid(int **arr, int dimension, int row, int col, int value, int row_per_block, int col_per_block) {
    if (arr[row][col] == value) {
        return 1;
    }
    if ((in_block(arr, row - row % row_per_block, col - col % col_per_block, value,row_per_block,col_per_block)
    || (in_row(arr[row],dimension,value))
    || (in_col(arr,dimension, col, value)))) {
        return 0;
    }
    return 1;
}
/*set erroneous board values*/
void fix_error(int **arr,int **error,int dimension,int row,int col,int value, int block_start_row, int block_start_col,int row_per_block,int col_per_block){
    int index_row,index_col,add_or_remove,is_valid=1;
    add_or_remove=value==0?0:1;

    for (index_col = 0; index_col < dimension; index_col++) {
        if (arr[row][index_col] == arr[row][col] && col != index_col) { error[row][index_col] = add_or_remove; is_valid=0;}
    }
    for (index_row = 0; index_row < dimension; index_row++) {
        if (arr[index_row][col] == arr[row][col]&&row!=index_row) { error[index_row][col] = add_or_remove; is_valid=0; }
    }
    for (index_row = 0; index_row < row_per_block; index_row++) {
        for (index_col = 0; index_col < col_per_block; index_col++) {
            if (arr[index_row + block_start_row][index_col + block_start_col] == arr[row][col]&&(!(col==index_col+block_start_col&&row==index_row+block_start_row))) {
                error[index_row + block_start_row][index_col + block_start_col] = add_or_remove; is_valid=0;
            }
        }
    }
    if(add_or_remove&&!is_valid)/* if we added erroneous */
        error[row][col]=1;
}


void reCalcErrors(Board *b){
    int **arr, **error;
    int dimension, row_per_block, col_per_block;
    int index_row, index_col;
    dimension = b->dimension;
    row_per_block = b->row_per_block;
    col_per_block = b->col_per_block;
    arr = b->arr;
    error = b->error;
    for(index_row=0; index_row<dimension; index_row++ ){
        for(index_col=0; index_col<dimension; index_col++){
            if(arr[index_row][index_col]!=0) {
                fix_error(arr, error, dimension, index_row, index_col, arr[index_row][index_col],
                          index_row - index_row % row_per_block, index_col - index_col % col_per_block, row_per_block,col_per_block);
            }
        }

    }
}
