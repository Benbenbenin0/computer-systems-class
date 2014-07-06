/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
  This a special function that is optimized for transposing 64-by-64 matrices.
  Don't call this function directly unless you know what you're doing.

  This function is described in a pdf that should come in the folder of this 
  file. Essentially we block the matrix into 8-by-8 blocks. We handle blocks
  on the diagonal and non-diagonal blocks separately. For non-diagonal blocks 
  we achieve the minimum possible no. of cache missed (16) by making use of 
  local variables (probably stored in registers) and B, for temporary storage.
*/
void special_transpose(int M, int N, int A[N][M], int B[M][N])
{
  REQUIRES (M == 64 && N == 64);
  int v1, v2, v3, v4, v5, v6, v7, v8; //Ideally, these go into the registers
  int i, j, k, tmp;

  for (i = 0; i < 64; i+=8)
  {
    for (j = 0; j < 64; j += 8)
    {
      //8-by-8 block on the diagonal. Then split into 4, 4-by-4
      //blocks
      if (i == j) {
        for (v1 = 0; v1 < 4; v1++)
        {
          for (v2 = 0; v2 < 4; v2++)
          {
            if (v1 == v2) continue;
            tmp = A[i+v1][j+v2];
            B[j+v2][i+v1] = tmp;
          }

          tmp = A[i+v1][i+v1];
          B[i+v1][i+v1] = tmp;
        }

        for (v1 = 4; v1 < 8; v1++)
        {
          for (v2 = 0; v2 < 4; v2++)
          {
            if ((v1-4) == v2) continue;
            tmp = A[i+v1][j+v2];
            B[j+v2][i+v1] = tmp;
          }

          tmp = A[i+v1][i+(v1-4)];
          B[i+(v1-4)][i+v1] = tmp;
        }

        for (v1 = 4; v1 < 8; v1++)
        {
          for (v2 = 4; v2 < 8; v2++)
          {
            if (v1 == v2) continue;
            tmp = A[i+v1][j+v2];
            B[j+v2][i+v1] = tmp;
          }

          tmp = A[i+v1][i+v1];
          B[i+v1][i+v1] = tmp;
        }

        for (v1 = 0; v1 < 4; v1++)
        {
          for (v2 = 4; v2 < 8; v2++)
          {
            if ((v1+4) == v2) continue;
            tmp = A[i+v1][j+v2];
            B[j+v2][i+v1] = tmp;
          }

          tmp = A[i+v1][i+v1+4];
          B[i+v1+4][i+v1] = tmp;
        }
      }

      //8-by-8 block not on diagonal
      //See pdf for info
      else {
        for (v1 = 0; v1 < 4; v1++) { //Picture 1
          for (v2 = 0; v2 < 4; v2++) {
            tmp = A[i+v1][j+v2];
            B[j+v2][i+v1] = tmp;
          }
        }

        //Picture 2
        v1 = A[i][j+4];
        v2 = A[i][j+5];
        v3 = A[i][j+6];
        v4 = A[i][j+7];
        v5 = A[i+1][j+4];
        v6 = A[i+1][j+5];
        v7 = A[i+1][j+6];
        v8 = A[i+1][j+7];

        for (k = 4; k < 8; k++) { //Picture 3
          tmp = A[i+2][j+k];
          B[j+2][i+k] = tmp;
          tmp = A[i+3][j+k];
          B[j+3][i+k] = tmp;
        }

        for (k = 4; k < 8; k++) { //Picture 4
            tmp = A[i+k][j+0];
            B[j+0][i+k] = tmp;
            tmp = A[i+k][j+1];
            B[j+1][i+k] = tmp;
        }

        for (k = 2; k < 4; k++) { //Picture 5
            tmp = B[j+k][i+4];
            B[j+4][i+k] = tmp;
            tmp = B[j+k][i+5];
            B[j+5][i+k] = tmp;
        }

        for (k = 4; k < 6; k++) { //Picture 6
            tmp = A[i+k][j+2];
            B[j+2][i+k] = tmp;
            tmp = A[i+k][j+3];
            B[j+3][i+k] = tmp;
        }

        //Picture 7
        B[j+4][i] = v1;
        B[j+4][i+1] = v5;
        B[j+5][i] = v2;
        B[j+5][i+1] = v6;

        //Picture 8
        v1 = B[j+2][i+6];
        v2 = B[j+2][i+7];
        v5 = B[j+3][i+6];
        v6 = B[j+3][i+7];

        for (k = 6; k < 8; k++) {
            tmp = A[i+k][j+2];
            B[j+2][i+k] = tmp;
            tmp = A[i+k][j+3];
            B[j+3][i+k] = tmp;
        }

        //Picture 10

        B[j+6][i] = v3;
        B[j+6][i+1] = v7;
        B[j+6][i+2] = v1;
        B[j+6][i+3] = v5;
        B[j+7][i] = v4;
        B[j+7][i+1] = v8;
        B[j+7][i+2] = v2;
        B[j+7][i+3] = v6;

        //Picture 11

        for (v1 = 4; v1 < 8; v1++) {
          for (v2 = 4; v2 < 8; v2++) {
            tmp = A[i+v1][j+v2];
            B[j+v2][i+v1] = tmp;
          }
        }
      }
    }
  }
}


/** Generic transpose based on blocking.
    User can control the block width and length.
    **/
void normal_transpose(int M, int N, int A[N][M], int B[M][N])
{
  REQUIRES(M > 0);
  REQUIRES(N > 0);

  int bheight = 32;
  int bwidth = 8;
  int i, j, k, l;

  for (i = 0; i < N; i += bheight)
  {
    int highRow = i+bheight;
    if (highRow > N) highRow = N;

    for (j = 0; j < M; j+= bwidth)
    {
      int highCol = j+bwidth;
      if (highCol > M) highCol = M;

      for (k = i; k < highRow; k++)
      {
        for (l = j; l < highCol; l++)
        {
          if (k == l) continue;
          int tmp = A[k][l];
          B[l][k] = tmp;
        }

        if (j <= k && k < highCol) {
          int tmp = A[k][k];
          B[k][k] = tmp;
        }
      }
    }
  }

  ENSURES(is_transpose(M, N, A, B));
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);
 
    if (M == 64 && N == 64) special_transpose(M,N,A,B);
    else normal_transpose(M,N,A,B);

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 
    
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}
