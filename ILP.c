#include "ILP.h"
#include "Board.h"
#include "ValidBoard.h"
#include "MainAux.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define GUROBI 1
#define SUPPRESS 0
#define NON_FEASIBLE "No feasible solution was found\n"
#if GUROBI

#include "gurobi_c.h"
#endif

int floorToInt(double x) {
    int cast = (int) x;
    return cast <= x ? cast : cast - 1;
}

int weightedRandom(double *arr, int len){
    int i;
    double r;
    double sum = 0;
    for(i=0; i < len; i++){
        sum = sum +arr[i];
        arr[i] = sum;
    }
    r = rand()/(double)RAND_MAX * sum;

    for(i=0; i < len; i++){
        if(arr[i]>=r)
            return i;
    }
    return 0;
}

Board *get_autofilled(Board *b) {
    int **arr;
    int dimension, row_per_block, col_per_block;
    int row, col, num, count, candidate = 0;
    Board *temp;
    dimension = b->dimension;
    row_per_block = b->row_per_block;
    col_per_block = b->col_per_block;
    arr = b->arr;
    temp = duplicateBoard(b);
    for (row = 0; row < dimension; row++) {
        for (col = 0; col < dimension; col++) {
            if (arr[row][col] == 0) {
                count = 0;
                for (num = 1; num < dimension + 1; num++) {
                    if (is_valid(arr, dimension, row, col, arr[row][col], row_per_block, col_per_block)) {
                        count++;
                        candidate = num;
                    }
                }
                if (count == 1) {
                    temp->arr[row][col] = candidate;
                }
            }
        }
    }
    return temp;
}

#if GUROBI


void freeTemporaryArrays(int *ind, double *val, int ***rowVars, int ***colVars, int ***blockVars, int **cellVars,
                         char *vType, double *solution, int dimension) {

    int i, j;
    free(ind);
    ind = NULL;
    free(solution);
    solution = NULL;
    free(val);
    val = NULL;
    free(vType);
    vType = NULL;
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            free(cellVars[(dimension * i) + j]);
            cellVars[(dimension * i) + j] = NULL;
            free(rowVars[i][j]);
            rowVars[i][j] = NULL;
            free(blockVars[i][j]);
            blockVars[i][j] = NULL;
            free(rowVars[i][j]);
            rowVars[i][j] = NULL;
        }
        free(rowVars[i]);
        rowVars[i] = NULL;
        free(blockVars[i]);
        blockVars[i] = NULL;
        free(rowVars[i]);
        rowVars[i] = NULL;
    }
    free(rowVars);
    rowVars = NULL;
    free(colVars);
    colVars = NULL;
    free(blockVars);
    blockVars = NULL;
    free(cellVars);
    cellVars = NULL;
}


Response *QUIT(GRBmodel *model, GRBenv *env, int error, double *mappedSolution, int status,
               int *ind, double *val, int ***rowVars, int ***colVars, int ***blockVars, int **cellVars,
               char *vType, double *solution,
               int dimension) {
    Response *res = (Response *) malloc(sizeof(Response));
    /* Free model */
    GRBfreemodel(model);

    /* Free environment */
    GRBfreeenv(env);

    res->solution = mappedSolution;

    if (error) {
#if DEBUG
        printf("ERROR: %s\n code: %d\n", GRBgeterrormsg(env), error);
#endif
        res->valid = 0;
        return res;
    }
    /*release temporary arrays used throughout the program*/
    freeTemporaryArrays(ind, val, rowVars, colVars, blockVars, cellVars, vType, solution, dimension);
    res->valid = (status == GRB_OPTIMAL);
    return res;
}

#endif

Response *calc(Board *src, enum LPMode mode) {
#if GUROBI
    GRBenv *env = NULL;
    GRBmodel *model = NULL;
#endif
    int *ind; /* should be of size dimension*dimension*dimension */
    double *val, *target; /* should be of size dimension */
    int ***rowVars; /* indices of variables, indexed by row and value */
    int ***colVars; /* indices of variables, indexed by col and value */
    int ***blockVars; /* indices of variables, indexed by block and value */
    int **cellVars; /* indices of variables, indexed by cell */
#if GUROBI
    char *vType; /* should be of size dimension*dimension*dimension */
    double *solution; /* will contain the assignments made to each variable */
    double *mappedSolution; /* will contain the assignments made to each variable, indexed as [row][col][val] */
    int optimStatus = -1;
    double objVal;
#endif
    int dimension, blockWidth, blockHeight;
    int i, j, v, blockIndex, indexInBlock, varCount, valueIsValid;
    int varConstraintIndex; /* the index of the currently processed variable within the currently constructed constraint*/
#if GUROBI
    int error = 0;
#endif
#if DEBUG
    int constraintCount = 0;
#endif
    Board *b = get_autofilled(src);
    dimension = b->dimension;
    blockWidth = b->row_per_block;
    blockHeight = b->col_per_block;

    /*array allocation*/
    ind = init_malloc(sizeof(int), (dimension * dimension * dimension), INT);
    target = init_malloc(sizeof(double), (dimension * dimension * dimension), INT);
    val = init_malloc(sizeof(double), (dimension * dimension * dimension), DOUBLE);
    cellVars = init_malloc(sizeof(int *), dimension * dimension, INT_POINTER);
    rowVars = init_malloc(sizeof(int *), dimension, INT_POINTER);
    colVars = init_malloc(sizeof(int *), dimension, INT_POINTER);
    blockVars = init_malloc(sizeof(int *), dimension, INT_POINTER);
#if GUROBI
    vType = malloc(sizeof(char) * (dimension * dimension * dimension));
    solution = malloc(sizeof(double) * (dimension * dimension * dimension));
    mappedSolution = malloc(sizeof(double) * (dimension * dimension * dimension));
#endif

    /* Create an empty model */
    varCount = 0;
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            /*each cell in the cellVars array contains an array of variables, one for each possible value*/
            cellVars[i * dimension + j] = init_malloc(sizeof(int), dimension, INT);
            blockIndex =
                    (int) floorToInt((double) dimension / blockWidth) * (int) floorToInt((double) j / blockHeight) +
                    (int) (i / blockWidth);
            indexInBlock = blockWidth * (j % blockHeight) + (i % blockWidth);
            for (v = 0; v < dimension; v++) {
                /*----------------ALLOCATION OF ARRAYS ON DEMAND------------------*/
                if (rowVars[i] == NULL) {
                    rowVars[i] = init_malloc(sizeof(int *), dimension, INT_POINTER);
                }
                if (colVars[j] == NULL) {
                    colVars[j] = init_malloc(sizeof(int *), dimension, INT_POINTER);
                }
                if (blockVars[blockIndex] == NULL) {
                    blockVars[blockIndex] = init_malloc(sizeof(int *), dimension, INT_POINTER);
                }
                if (rowVars[i][v] == NULL) {
                    rowVars[i][v] = init_malloc(sizeof(int), dimension, INT);
                }
                if (colVars[j][v] == NULL) {
                    colVars[j][v] = init_malloc(sizeof(int), dimension, INT);
                }
                if (blockVars[blockIndex][v] == NULL) {
                    blockVars[blockIndex][v] = init_malloc(sizeof(int), dimension, INT);
                }
                /*----------------ALLOCATION END------------------*/
                valueIsValid = (!b->arr[i][j]) && is_valid(b->arr, dimension, i, j, v + 1, blockHeight, blockWidth);
                if (valueIsValid) {
                    cellVars[i * dimension + j][v] = varCount;
                    rowVars[i][v][j] = varCount;
                    colVars[j][v][i] = varCount;
                    blockVars[blockIndex][v][indexInBlock] = varCount;
                    target[varCount] = mode == BinaryVars ? 0 : rand() % (dimension * dimension);
                    DEBUG_PRINT(("i=%d, j=%d, v=%d  ->   %d\n", i, j, v, varCount));
                    varCount++;
                } else {
                    cellVars[i * dimension + j][v] = -1;
                    rowVars[i][v][j] = -1;
                    colVars[j][v][i] = -1;
                    blockVars[blockIndex][v][indexInBlock] = -1;
                }
            }
        }
    }

#if GUROBI
    for (i = 0; i < varCount; i++) {
        if (mode == BinaryVars) {
            vType[i] = GRB_BINARY;
        }
        if (mode == ContinuousVars) {
            vType[i] = GRB_CONTINUOUS;
        }
    }

    /* Create environment */
    error = GRBloadenv(&env, "sudoku.log");
    if (error) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }

    /* Create new model */
    error = GRBnewmodel(env, &model, "sudoku", varCount, target, NULL, NULL, vType, NULL);
    if (error) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }

    /* Disable Logging */
    error = GRBsetintparam(GRBgetenv(model), "LogToConsole", 0);
    if (error) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }
#endif
    /*-----------CONSTRAINTS-----------*/
#if DEBUG
    constraintCount = 0;
#endif
    /* for the LP case add constraints to each variable, limiting it to a 0 to 1 range */
    if (mode == ContinuousVars) {
        val[0] = 1.0;
        DEBUG_PRINT(("***Binary Constraints***\n"));
        for (i = 0; i < varCount; i++) {
#if GUROBI
            error = GRBaddconstr(model, 1, &i, val, GRB_LESS_EQUAL, 1.0, NULL);
            if (error) {
                return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars,
                            cellVars, vType, solution, dimension);
            }
#if DEBUG
            DEBUG_PRINT(("constraint %d\n", constraintCount));
            constraintCount++;
#endif
            error = GRBaddconstr(model, 1, &i, val, GRB_GREATER_EQUAL, 0.0, NULL);
            if (error) {
                return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars,
                            cellVars, vType, solution, dimension);
            }
#if DEBUG
            DEBUG_PRINT(("constraint %d\n", constraintCount));
            constraintCount++;
#endif
#endif
        }
    }

    /* limit cells to one value */
    DEBUG_PRINT(("***Cell Constraints***\n"));
    /* iterate over rows */
    for (i = 0; i < dimension; i++) {
        /* iterate over columns */
        for (j = 0; j < dimension; j++) {
            varConstraintIndex = 0;
            /* iterate over values */
            if (b->arr[i][j]==0) {
                for (v = 0; v < dimension; v++) {
                    if (cellVars[(i * dimension) + j][v] != -1) {
                        ind[varConstraintIndex] = cellVars[i * dimension + j][v];
                        val[varConstraintIndex] = 1.0;
                        varConstraintIndex++;
                    }
                }
#if GUROBI
                DEBUG_PRINT(("val in %d, %d is %d\n", i, j, b->arr[i][j]));
                /*varConstraintIndex = (varConstraintIndex - 1) < 0 ? 0 : (varConstraintIndex - 1);*/
                error = GRBaddconstr(model, varConstraintIndex, ind, val, GRB_EQUAL, 1.0, NULL);
                if (error) {
                    return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars,
                                cellVars, vType, solution, dimension);
                }
#if DEBUG
                DEBUG_PRINT(("constraint %d\n", constraintCount));
                constraintCount++;
#endif
#endif
            }
        }
    }

    /* limit each value to one appearance per row */
    DEBUG_PRINT(("***Row Constraints***\n"));
    /* iterate over rows */
    for (i = 0; i < dimension; i++) {
        /* iterate over values */
        for (v = 0; v < dimension; v++) {
            varConstraintIndex = 0;
            if (!in_row(b->arr[i], b->dimension, v+1)) {
                /* iterate over columns */
                for (j = 0; j < dimension; j++) {
                    if (rowVars[i][v][j] != -1) {
                        ind[varConstraintIndex] = rowVars[i][v][j];
                        val[varConstraintIndex] = 1.0;
                        varConstraintIndex++;
                    }
                }
#if GUROBI
                /*varConstraintIndex = (varConstraintIndex - 1) < 0 ? 0 : (varConstraintIndex - 1);*/
                if (varConstraintIndex) {
                    error = GRBaddconstr(model, varConstraintIndex, ind, val, GRB_EQUAL, 1.0, NULL);
                    if (error) {
                        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars,
                                    blockVars,
                                    cellVars, vType, solution, dimension);
                    }
#if DEBUG
                    DEBUG_PRINT(("constraint %d\n", constraintCount));
                    constraintCount++;
#endif
                }
#endif
            }
        }
    }

    /* limit each value to one appearance per col */
    DEBUG_PRINT(("***Coll Constraints***\n"));
    /* iterate over columns */
    for (j = 0; j < dimension; j++) {
        /* iterate over values */
        for (v = 0; v < dimension; v++) {
            varConstraintIndex = 0;
            if (!in_col(b->arr, b->dimension, j, v+1)) {
                /* iterate over rows */
                for (i = 0; i < dimension; i++) {
                    if (colVars[j][v][i] != -1) {
                        ind[varConstraintIndex] = colVars[j][v][i];
                        val[varConstraintIndex] = 1.0;
                        varConstraintIndex++;
                    }
                }
#if GUROBI
                if (varConstraintIndex) {
                    error = GRBaddconstr(model, varConstraintIndex, ind, val, GRB_EQUAL, 1.0, NULL);
                    if (error) {
                        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars,
                                    blockVars,
                                    cellVars, vType, solution, dimension);
                    }
#if DEBUG
                    DEBUG_PRINT(("constraint %d\n", constraintCount));
                    constraintCount++;
#endif
                }
#endif
            }
        }
    }

    /* limit each value to one appearance per block */
    /* iterate over blocks */
    for (blockIndex = 0; blockIndex < dimension; blockIndex++) {
        /* iterate over values */
        for (v = 0; v < dimension; v++) {
            varConstraintIndex = 0;
            /*TODO - calculate i and j to be the indices of the first cell in the block*/
            i = blockIndex % (dimension/blockWidth) * blockWidth;
            j = floorToInt(blockIndex / (dimension/blockWidth)) * blockHeight;
            if (!in_block(b->arr, i, j, v+1, b->row_per_block, b->col_per_block)) {
                /* iterate over cells within block */
                for (indexInBlock = 0; indexInBlock < dimension; indexInBlock++) {
                    if (blockVars[blockIndex][v][indexInBlock] != -1) {
                        ind[varConstraintIndex] = blockVars[blockIndex][v][indexInBlock];
                        val[varConstraintIndex] = 1.0;
                        varConstraintIndex++;
                    }
                }
#if GUROBI
                if (varConstraintIndex) {
                    error = GRBaddconstr(model, varConstraintIndex, ind, val, GRB_EQUAL, 1.0, NULL);
                    if (error) {
                        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars,
                                    blockVars,
                                    cellVars, vType, solution, dimension);
                    }
#if DEBUG
                    DEBUG_PRINT(("constraint %d\n", constraintCount));
                    constraintCount++;
#endif
                }
#endif
            }
        }
    }

#if GUROBI
    /* Optimize model */
    error = GRBoptimize(model);
    if (error) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }

#if DEBUG
    /* Write model to 'sudoku.lp' */
    error = GRBwrite(model, "sudoku.lp");
    if (error) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }
#endif

    /* Capture solution information */
    error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimStatus);
    if (error || optimStatus == GRB_INF_OR_UNBD) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }


    error = GRBgetdblattrarray(model, GRB_DBL_ATTR_X, 0, varCount, solution);
    if (error) {
        return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars,
                    vType, solution, dimension);
    }

    /* iterate over rows */
    for (i = 0; i < dimension; i++) {
        /* iterate over columns */
        for (j = 0; j < dimension; j++) {
            /* iterate over values */
            for (v = 0; v < dimension; v++) {
                if (cellVars[(i * dimension) + j][v] != -1) {
                    mappedSolution[(i * dimension * dimension) + (j * dimension) + v] = solution[cellVars[
                            (i * dimension) + j][v]];
                } else {
                    mappedSolution[(i * dimension * dimension) + (j * dimension) + v] = -1;
                }
            }
        }
    }
#if DEBUG
    printf("variables assignments: \n");
    for (i = 0; i < varCount; i++)
        printf("%f ", solution[i]);
    printf("\n\n");
#endif
    return QUIT(model, env, error, mappedSolution, optimStatus, ind, val, rowVars, colVars, blockVars, cellVars, vType,
                solution, dimension);
#endif
#if !GUROBI
    return NULL;
#endif
}

int validateLP(Board *b) {
    Response *res;
    res = calc(b, BinaryVars);
    if(!res)
        return 0;
    if (res->solution)
        free(res->solution);
    free(res);
    return res->valid;
}

void guessLP(Board *b, double threshold) {
    Response *res;
    double *solution;
    int i, j, v, d, selectedValueIndex;
    int overThresholdCount;/*the amount of values matched to the current cell with a probability over the threshold*/
    int *overThresholdVals;/*the values matched to the current cell with a probability over the threshold*/
    double *overThresholdProb;/*the probabilities attached to each value with a probability over the threshold*/
    res = calc(b, ContinuousVars);
    if (!res || !res->valid) {
        printf(NON_FEASIBLE);
        if (res->solution)
            free(res->solution);
        free(res);
        return;
    }
    d = b->dimension;
    solution = res->solution;
    overThresholdVals = malloc(sizeof(int) * d);
    overThresholdProb = malloc(sizeof(double) * d);
    /* iterate over rows */
    for (i = 0; i < d; i++) {
        /* iterate over columns */
        for (j = 0; j < d; j++) {
            /* iterate over values */
            overThresholdCount = 0;
            for (v = 0; v < d; v++) {
                if (solution[(i * d * d) + (j * d) + v] != -1) {
                    if(is_valid(b->arr, d, i, j, (v+1), b->row_per_block, b->col_per_block)) {
                        if (solution[(i * d * d) + (j * d) + v] >= threshold) {
                            overThresholdVals[overThresholdCount] = v;
                            overThresholdProb[overThresholdCount] = solution[(i * d * d) + (j * d) + v];
                            overThresholdCount++;
                        }
                    }
                }
            }
            if(overThresholdCount>0){
                selectedValueIndex = weightedRandom(overThresholdProb, overThresholdCount);
                printf("%d vals over %f threshold, the %d was selected, which is %d\n", overThresholdCount, threshold, selectedValueIndex, overThresholdVals[selectedValueIndex] + 1);
                b->arr[i][j] = overThresholdVals[selectedValueIndex] + 1;
            }
        }
    }
    free(overThresholdVals);
    free(overThresholdProb);
    if (res->solution)
        free(res->solution);
    free(res);
}

void generateLP(Board *b, int **resArr, int *valid) {
    Response *res;
    double *solution;
    int i, j, v, dimension;
    res = calc(b, BinaryVars);
    if (!res)
        return;
    if (!res->valid) {
        *valid = 0;
        free(res->solution);
        free(res);
        return;
    }
    dimension = b->dimension;
    solution = res->solution;
    /* iterate over rows */
    for (i = 0; i < dimension; i++) {
        /* iterate over columns */
        for (j = 0; j < dimension; j++) {
            /* iterate over values */
            for (v = 0; v < dimension; v++) {
                if (solution[(i * dimension * dimension) + (j * dimension) + v] != -1) {
                    if ((int) solution[(i * dimension * dimension) + (j * dimension) + v]) {
                        resArr[i][j] = v + 1;
                    }
                } else {
                    resArr[i][j] = b->arr[i][j];
                }
            }
        }
    }
    if (res->solution)
        free(res->solution);
    if (res != NULL)
        free(res);
}

int hintLP(Board *b, int x, int y) {
    Response *res;
    double *solution;
    int v, dimension, hint;
    hint = 0;
    res = calc(b, BinaryVars);
    if (!res) {
        return 0;
    }
    if (!res->valid) {
        printf(NON_FEASIBLE);
        if (res->solution)
            free(res->solution);
        free(res);
        return 0;
    }
    dimension = b->dimension;
    solution = res->solution;
    /* iterate over rows */
    for (v = 0; v < dimension; v++) {
        if (solution[(x * dimension * dimension) + (y * dimension) + v] != -1) {
            if ((int) (solution[(x * dimension * dimension) + (y * dimension) + v])) {
                hint = v + 1;
            }
        }
    }
    if (res->solution)
        free(res->solution);
    free(res);
    return hint;
}

void guessHintLP(Board *b, int x, int y) {
    Response *res;
    double *solution;
    int v, dimension;
    res = calc(b, BinaryVars);
    if (!res)
        return;
    if (!res->valid) {
        printf(NON_FEASIBLE);
        if (res->solution)
            free(res->solution);
        free(res);
        return;
    }
    dimension = b->dimension;
    solution = res->solution;
    /* iterate over rows */
    for (v = 0; v < dimension; v++) {
        if (solution[(x * dimension * dimension) + (y * dimension) + v] > 0) {
            printf("cell [%d, %d] has value %d with certainty %f\n", y + 1, x + 1, v + 1,
                   solution[(x * dimension * dimension) + (y * dimension) + v]);
        }
    }
    if (res->solution)
        free(res->solution);
    free(res);
}
