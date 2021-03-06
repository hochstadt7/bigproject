#include <stdio.h>
#include <stdlib.h>
#include "Board.h"
#include "MainAux.h"

List *create_list() {
    List *tmp = (List *) malloc(sizeof(List));
    if(tmp==NULL){
        printf("List allocation failed.\n");
        exit(0);
    }
    tmp->head = NULL;
    tmp->curr = NULL;
    return tmp;
}
Node* create_node(Board* b){
    Node *temp= (Node *) malloc(sizeof(Node));
    if(temp==NULL) {
        printf("Node allocation failed.\n");
        exit(0);
    }
    temp->arr = duplicateArray(b->arr, b->dimension);
    temp->next=NULL; temp->prev=NULL;
    return temp;
}

/*add move to the move's list*/
Node* add(Board *b){
    List *lst = b->lst;
    Node *tmp;
    Node* node=create_node(b);
    if(lst->head==NULL){
        lst->head=node;
        lst->curr=node;
    }
    else{
        if(lst->curr!=NULL) {/*maybe curr is null!*/
            tmp = lst->curr->next;
            if (tmp != NULL) {/*the redo part of the list is cleared*/
                while (tmp->next != NULL) {
                    free(tmp->arr);
                    free(tmp);
                    tmp = tmp->next;
                }
                if(tmp) {
                    free(tmp->arr);
                    free(tmp);
                }
            }
            lst->curr->next = node;
            node->prev = lst->curr;
            lst->curr = lst->curr->next;
        }
        else{
            lst->head=node;
            lst->curr=node;
        }
    }
    return node;
}

/*free list resources*/
void free_lst(List *lst){
    if(lst==NULL)
    {
        return;
    }
    if(lst->head==NULL)
    {
        free(lst);
        return;
    }
    while (lst->head->next!=NULL) {
        lst->head = lst->head->next;
        free(lst->head->prev);
    }
    free(lst->head);
    free(lst);
}
/*create board based on the given parameters*/
Board* create_board(int dimension,int row_per_block,int col_per_block){
    Board* board=(Board*) malloc(sizeof(Board));
    if(!board) {
        printf("board creation failed.\n");
        exit(0);
    }
    board->arr=first_init(dimension);
    board->fixed=first_init(dimension);
    board->error=first_init(dimension);
    board->lst=create_list();
    board->dimension=dimension;board->mark_errors=1;
    board->row_per_block=row_per_block;
    board->col_per_block=col_per_block;
    board->mode=InitMode;
    return board;
}
/*free all board resources*/
void destroy_board(Board* board){
    if(!board)
        return;
    free_lst(board->lst);
    board->lst = NULL;
    free_arrays(board->error,board->dimension);
    board->error = NULL;
    free_arrays(board->fixed,board->dimension);
    board->fixed = NULL;
    free_arrays(board->arr,board->dimension);
    board->arr = NULL;
    free(board);
}
/*printing the board according to the specified format*/
void print_board(Board* b){
    int **arr, **fixed, **error;
    int dimension, row_per_block, col_per_block;
    int index_row, index_col, index_block, blocks_per_row,num_dash;
    int mark_errors;
    dimension = b->dimension;
    row_per_block = b->row_per_block;
    col_per_block = b->col_per_block;
    arr = b->arr;
    fixed = b->fixed;
    error = b->error;
    mark_errors = (b->mark_errors || (b->mode == SolveMode));
    blocks_per_row = dimension /col_per_block;
    num_dash=4*dimension+row_per_block+1;
    for (index_row = 0; index_row < dimension; index_row++) {
        if (index_row % row_per_block == 0) {
            for(index_block=0; index_block<num_dash; index_block++) {
                printf("-"); /* block separators */
            }
            printf("\n");
        }
        for (index_block = 0; index_block < blocks_per_row; index_block++) {
            printf("|"); /* block separators */
            for (index_col = 0; index_col < col_per_block; index_col++) {
                if (arr[index_row][index_col + col_per_block * index_block] != 0) {
                    /* filled cells*/
                    if (fixed[index_row][index_col + col_per_block * index_block] != 0){
                        /*fixed cells*/
                        printf(" %2d.", arr[index_row][index_col + col_per_block * index_block]);
                    }
                    else if(error[index_row][index_col + col_per_block * index_block] != 0 && (b->mode == EditMode || mark_errors)){
                        /*cells involved in errors*/
                        printf(" %2d*", arr[index_row][index_col + col_per_block * index_block]);
                    }
                    else{
                        /* 'regular' cells' */
                        printf(" %2d ", arr[index_row][index_col + col_per_block * index_block]);
                    }
                } else {
                    /* empty cells */
                    printf("    "); /*empty cell*/
                }
            }
        }
        printf("|\n");
    }
    for(index_block=0; index_block<num_dash; index_block++) {
        printf("-");
    }
    printf("\n");
}
Board* duplicateBoard(Board* b){
    int dimension, row_per_block, col_per_block, i, j;
    Board *new;
    if(b == NULL){
        return NULL;
    }
    dimension = b->dimension;
    row_per_block = b->row_per_block;
    col_per_block = b->col_per_block;
    new = create_board(dimension, row_per_block, col_per_block);
    for(i=0; i<dimension; i++){
        for(j=0; j<dimension; j++){
            new->arr[i][j] = b->arr[i][j];
        }
    }
    return new;
}
