// Lab 2 - Single-core CPU optimizations, blur kernel
// Author: Sheharyar (21506344)
//
// All times were measured on Dalek with:
//   ./run -k blur --load-image img/1024.png -i 100 -v seq -wt <tiling> -n
// Sections 2.1 to 2.7 are at -O0 on the az4-mixed partition. From 2.7 the runs
// are pinned to node az4-a7900-2, because az4-mixed gave two distinct groups of
// timings. Section 2.8 compares the optimization levels and 2.9 the four CPU
// types of the cluster; both tables are at the end of the file.
//
// Output checked every time with --show-hash:
//   default, optim5, optim6      -> 19104192581038f797df369fa2606686db1b682d4ce29353572a341ba88ebb55
//   default_nb, optim1 .. optim4 -> 8c55e21781f2b5135d41e4ebccade6dd4a5d9f1a7ec4258f09744d5ab9b459c6

#include "easypap.h"

#include <omp.h>

///////////////////////////// Sequential version (tiled)
// Suggested cmdline(s):
// ./run -l data/img/1024.png -k blur -v seq -si
//

// --- 1.3 Compiler optimization levels (tiling default) ---
// -O0 : 5575.976 ms
// -O1 : 1582.819 ms
// -O2 : 1205.489 ms
// -O3 : 1036.087 ms
// The compiler on its own already divides the time by 5.4 without a single
// line of source being changed. Everything below is measured at -O0 so that
// my own transformations are the only thing moving.

int blur_do_tile_default (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++) {
      unsigned r = 0, g = 0, b = 0, a = 0, n = 0;

      int i_d = (i > 0) ? i - 1 : i;
      int i_f = (i < DIM - 1) ? i + 1 : i;
      int j_d = (j > 0) ? j - 1 : j;
      int j_f = (j < DIM - 1) ? j + 1 : j;

      for (int yloc = i_d; yloc <= i_f; yloc++)
        for (int xloc = j_d; xloc <= j_f; xloc++) {
          unsigned c = cur_img (yloc, xloc);
          r += ezv_c2r (c);
          g += ezv_c2g (c);
          b += ezv_c2b (c);
          a += ezv_c2a (c);
          n += 1;
        }

      r /= n;
      g /= n;
      b /= n;
      a /= n;

      next_img (i, j) = ezv_rgba (r, g, b, a);
    }

  return 0;
}

// --- 2.1 default_nb : borders copied instead of computed ---
// -wt default_nb, -O0 : 5311.591 ms   (default was 5575.976 ms, so 1.05x)
// Only 4092 pixels out of 1048576 are on a border, so the 4 tests that the
// default version does on every pixel are almost always useless. Removing
// them gains 5%, which is little. The point of this version is that the
// neighbourhood is now always exactly 3x3, which the unrollings below need.
int blur_do_tile_default_nb (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++) {

      // Borders: simple copy of the previous pixel
      if (i == 0 || i == (int)DIM - 1 || j == 0 || j == (int)DIM - 1) {
        next_img (i, j) = cur_img (i, j);
        continue;
      }

      unsigned r = 0, g = 0, b = 0, a = 0;

      for (int yloc = i - 1; yloc <= i + 1; yloc++)
        for (int xloc = j - 1; xloc <= j + 1; xloc++) {
          unsigned c = cur_img (yloc, xloc);
          r += ezv_c2r (c);
          g += ezv_c2g (c);
          b += ezv_c2b (c);
          a += ezv_c2a (c);
        }

      next_img (i, j) = ezv_rgba (r / 9, g / 9, b / 9, a / 9);
    }

  return 0;
}

// --- 2.2 optim1 : x-axis loop unrolled ---
// -wt optim1, -O0 : 5140.100 ms   (3 runs: 5108 / 5111 / 5168)
// default_nb was 5311.591 ms, so 1.03x. The xloc loop always did exactly 3
// iterations, so I wrote them out and the counter, the test and the jump are
// gone. I expected more than 3%, so the loop control was not where the time
// was going.
int blur_do_tile_optim1 (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++) {

      // Borders: simple copy of the previous pixel
      if (i == 0 || i == (int)DIM - 1 || j == 0 || j == (int)DIM - 1) {
        next_img (i, j) = cur_img (i, j);
        continue;
      }

      unsigned r = 0, g = 0, b = 0, a = 0;

      for (int yloc = i - 1; yloc <= i + 1; yloc++) {
        unsigned c0 = cur_img (yloc, j - 1);   // left neighbour
        unsigned c1 = cur_img (yloc, j    );   // the pixel itself
        unsigned c2 = cur_img (yloc, j + 1);   // right neighbour

        r += ezv_c2r (c0) + ezv_c2r (c1) + ezv_c2r (c2);
        g += ezv_c2g (c0) + ezv_c2g (c1) + ezv_c2g (c2);
        b += ezv_c2b (c0) + ezv_c2b (c1) + ezv_c2b (c2);
        a += ezv_c2a (c0) + ezv_c2a (c1) + ezv_c2a (c2);
      }

      next_img (i, j) = ezv_rgba (r / 9, g / 9, b / 9, a / 9);
    }

  return 0;
}

// --- 2.3 optim2 : y-axis loop unrolled ---
// -wt optim2, -O0 : 5097 / 5155 / 5160 ms
// optim1          : 5108 / 5111 / 5168 ms
// I ran both three times because the first optim2 measurement came out higher
// than optim1. The two ranges overlap, so at -O0 this version changes nothing.
// Unrolling yloc keeps 9 pixels alive at the same time instead of 3, and at
// -O0 they all sit on the stack, so the extra loads and stores eat whatever
// the loop control saved. The 37 function calls per pixel are the real cost,
// which is what 2.4 attacks. This version does become useful once the
// optimizer is on, see the table in 2.8.
int blur_do_tile_optim2 (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++) {

      // Borders: simple copy
      if (i == 0 || i == (int)DIM - 1 || j == 0 || j == (int)DIM - 1) {
        next_img (i, j) = cur_img (i, j);
        continue;
      }

      // The 9 pixels of the 3x3 neighbourhood, read once each
      unsigned c00 = cur_img (i - 1, j - 1);
      unsigned c01 = cur_img (i - 1, j    );
      unsigned c02 = cur_img (i - 1, j + 1);
      unsigned c10 = cur_img (i    , j - 1);
      unsigned c11 = cur_img (i    , j    );
      unsigned c12 = cur_img (i    , j + 1);
      unsigned c20 = cur_img (i + 1, j - 1);
      unsigned c21 = cur_img (i + 1, j    );
      unsigned c22 = cur_img (i + 1, j + 1);

      unsigned r = ezv_c2r (c00) + ezv_c2r (c01) + ezv_c2r (c02)
                 + ezv_c2r (c10) + ezv_c2r (c11) + ezv_c2r (c12)
                 + ezv_c2r (c20) + ezv_c2r (c21) + ezv_c2r (c22);

      unsigned g = ezv_c2g (c00) + ezv_c2g (c01) + ezv_c2g (c02)
                 + ezv_c2g (c10) + ezv_c2g (c11) + ezv_c2g (c12)
                 + ezv_c2g (c20) + ezv_c2g (c21) + ezv_c2g (c22);

      unsigned b = ezv_c2b (c00) + ezv_c2b (c01) + ezv_c2b (c02)
                 + ezv_c2b (c10) + ezv_c2b (c11) + ezv_c2b (c12)
                 + ezv_c2b (c20) + ezv_c2b (c21) + ezv_c2b (c22);

      unsigned a = ezv_c2a (c00) + ezv_c2a (c01) + ezv_c2a (c02)
                 + ezv_c2a (c10) + ezv_c2a (c11) + ezv_c2a (c12)
                 + ezv_c2a (c20) + ezv_c2a (c21) + ezv_c2a (c22);

      next_img (i, j) = ezv_rgba (r / 9, g / 9, b / 9, a / 9);
    }

  return 0;
}

// --- 2.4 optim3 : ezv_c2r/g/b/a and ezv_rgba inlined by hand ---
// -wt optim3, -O0 : 2037 / 2048 / 2166 / 2166 ms
// optim2 was around 5155 ms, so 2.5x. This is where the time actually was.
// These functions are declared inline in lib/ezv/include/ezv_rgba.h, but gcc
// does not inline at -O0, so each of the 37 calls per pixel pays a whole call
// sequence to do one shift and one mask. Writing (c >> k) & 0xFF by hand
// removes all of them.
int blur_do_tile_optim3 (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++) {

      if (i == 0 || i == (int)DIM - 1 || j == 0 || j == (int)DIM - 1) {
        next_img (i, j) = cur_img (i, j);
        continue;
      }

      unsigned c00 = cur_img (i - 1, j - 1);
      unsigned c01 = cur_img (i - 1, j    );
      unsigned c02 = cur_img (i - 1, j + 1);
      unsigned c10 = cur_img (i    , j - 1);
      unsigned c11 = cur_img (i    , j    );
      unsigned c12 = cur_img (i    , j + 1);
      unsigned c20 = cur_img (i + 1, j - 1);
      unsigned c21 = cur_img (i + 1, j    );
      unsigned c22 = cur_img (i + 1, j + 1);

      unsigned r = ( c00        & 0xFF) + ( c01        & 0xFF) + ( c02        & 0xFF)
                 + ( c10        & 0xFF) + ( c11        & 0xFF) + ( c12        & 0xFF)
                 + ( c20        & 0xFF) + ( c21        & 0xFF) + ( c22        & 0xFF);

      unsigned g = ((c00 >>  8) & 0xFF) + ((c01 >>  8) & 0xFF) + ((c02 >>  8) & 0xFF)
                 + ((c10 >>  8) & 0xFF) + ((c11 >>  8) & 0xFF) + ((c12 >>  8) & 0xFF)
                 + ((c20 >>  8) & 0xFF) + ((c21 >>  8) & 0xFF) + ((c22 >>  8) & 0xFF);

      unsigned b = ((c00 >> 16) & 0xFF) + ((c01 >> 16) & 0xFF) + ((c02 >> 16) & 0xFF)
                 + ((c10 >> 16) & 0xFF) + ((c11 >> 16) & 0xFF) + ((c12 >> 16) & 0xFF)
                 + ((c20 >> 16) & 0xFF) + ((c21 >> 16) & 0xFF) + ((c22 >> 16) & 0xFF);

      unsigned a = ((c00 >> 24) & 0xFF) + ((c01 >> 24) & 0xFF) + ((c02 >> 24) & 0xFF)
                 + ((c10 >> 24) & 0xFF) + ((c11 >> 24) & 0xFF) + ((c12 >> 24) & 0xFF)
                 + ((c20 >> 24) & 0xFF) + ((c21 >> 24) & 0xFF) + ((c22 >> 24) & 0xFF);

      r /= 9; g /= 9; b /= 9; a /= 9;

      next_img (i, j) = r | (g << 8) | (b << 16) | (a << 24);
    }

  return 0;
}

// --- 2.5 optim4 : rotation of variables on the x-axis ---
// -wt optim4, -O0 : 1334 / 1335 / 1365 / 1517 ms
// optim3 was around 2050 ms, so 1.5x. Going from pixel j to pixel j+1, the
// middle and right columns of the window become the left and middle columns
// of the next one, so I keep them in variables and read only the new right
// column: 3 loads per pixel instead of 9. The 1517 ms run landed on a busy
// node, the three others agree.
//
// Total so far at -O0: 5575.976 ms for the original code down to about
// 1335 ms, 4.2x.
int blur_do_tile_optim4 (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++) {

    // Border rows: full copy
    if (i == 0 || i == (int)DIM - 1) {
      for (int j = x; j < x + width; j++)
        next_img (i, j) = cur_img (i, j);
      continue;
    }

    int j = x;

    // First column of the image is a border: copy it and start at j = 1
    if (j == 0) {
      next_img (i, 0) = cur_img (i, 0);
      j = 1;
    }

    // Preload the two columns left of the first pixel we compute
    unsigned cL0 = cur_img (i - 1, j - 1), cL1 = cur_img (i, j - 1), cL2 = cur_img (i + 1, j - 1);
    unsigned cM0 = cur_img (i - 1, j    ), cM1 = cur_img (i, j    ), cM2 = cur_img (i + 1, j    );

    for (; j < x + width; j++) {

      // Last column of the image is a border: copy it
      if (j == (int)DIM - 1) {
        next_img (i, j) = cur_img (i, j);
        continue;
      }

      // Only the new right column is read from memory
      unsigned cR0 = cur_img (i - 1, j + 1);
      unsigned cR1 = cur_img (i    , j + 1);
      unsigned cR2 = cur_img (i + 1, j + 1);

      unsigned sr = ( cL0        & 0xFF) + ( cL1        & 0xFF) + ( cL2        & 0xFF)
                  + ( cM0        & 0xFF) + ( cM1        & 0xFF) + ( cM2        & 0xFF)
                  + ( cR0        & 0xFF) + ( cR1        & 0xFF) + ( cR2        & 0xFF);

      unsigned sg = ((cL0 >>  8) & 0xFF) + ((cL1 >>  8) & 0xFF) + ((cL2 >>  8) & 0xFF)
                  + ((cM0 >>  8) & 0xFF) + ((cM1 >>  8) & 0xFF) + ((cM2 >>  8) & 0xFF)
                  + ((cR0 >>  8) & 0xFF) + ((cR1 >>  8) & 0xFF) + ((cR2 >>  8) & 0xFF);

      unsigned sb = ((cL0 >> 16) & 0xFF) + ((cL1 >> 16) & 0xFF) + ((cL2 >> 16) & 0xFF)
                  + ((cM0 >> 16) & 0xFF) + ((cM1 >> 16) & 0xFF) + ((cM2 >> 16) & 0xFF)
                  + ((cR0 >> 16) & 0xFF) + ((cR1 >> 16) & 0xFF) + ((cR2 >> 16) & 0xFF);

      unsigned sa = ((cL0 >> 24) & 0xFF) + ((cL1 >> 24) & 0xFF) + ((cL2 >> 24) & 0xFF)
                  + ((cM0 >> 24) & 0xFF) + ((cM1 >> 24) & 0xFF) + ((cM2 >> 24) & 0xFF)
                  + ((cR0 >> 24) & 0xFF) + ((cR1 >> 24) & 0xFF) + ((cR2 >> 24) & 0xFF);

      next_img (i, j) = (sr / 9) | ((sg / 9) << 8) | ((sb / 9) << 16) | ((sa / 9) << 24);

      // Rotation: shift the window one column to the right
      cL0 = cM0; cL1 = cM1; cL2 = cM2;
      cM0 = cR0; cM1 = cR1; cM2 = cR2;
    }
  }

  return 0;
}

// Original border handling, used only for the ~4000 edge pixels
static void blur_border_pixel (int i, int j)
{
  unsigned r = 0, g = 0, b = 0, a = 0, n = 0;

  int i_d = (i > 0)           ? i - 1 : i;
  int i_f = (i < (int)DIM - 1) ? i + 1 : i;
  int j_d = (j > 0)           ? j - 1 : j;
  int j_f = (j < (int)DIM - 1) ? j + 1 : j;

  for (int yloc = i_d; yloc <= i_f; yloc++)
    for (int xloc = j_d; xloc <= j_f; xloc++) {
      unsigned c = cur_img (yloc, xloc);
      r += ezv_c2r (c);
      g += ezv_c2g (c);
      b += ezv_c2b (c);
      a += ezv_c2a (c);
      n += 1;
    }

  next_img (i, j) = ezv_rgba (r / n, g / n, b / n, a / n);
}

// --- 2.6 optim5 : optim4 + original border handling restored ---
// -wt optim5, -O0 : 1356 / 1357 / 1551 / 1552 ms
// optim4          : 1334 / 1335 / 1365 / 1517 ms
// Same cost as optim4, and the hash is back to 19104192..., so the output is
// again identical to blur_do_tile_default. The borders go through the original
// code in blur_border_pixel, which is slow but runs on 4092 pixels only, and
// the main loop is left alone.
// The runs fall into two groups (around 1355 and around 1550 ms) depending on
// which node of az4-mixed I got, so from 2.7 on I pinned the node.
int blur_do_tile_optim5 (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++) {

    // Border rows: computed with the original code
    if (i == 0 || i == (int)DIM - 1) {
      for (int j = x; j < x + width; j++)
        blur_border_pixel (i, j);
      continue;
    }

    int j = x;

    if (j == 0) {
      blur_border_pixel (i, 0);
      j = 1;
    }

    unsigned cL0 = cur_img (i - 1, j - 1), cL1 = cur_img (i, j - 1), cL2 = cur_img (i + 1, j - 1);
    unsigned cM0 = cur_img (i - 1, j    ), cM1 = cur_img (i, j    ), cM2 = cur_img (i + 1, j    );

    for (; j < x + width; j++) {

      if (j == (int)DIM - 1) {
        blur_border_pixel (i, j);
        continue;
      }

      unsigned cR0 = cur_img (i - 1, j + 1);
      unsigned cR1 = cur_img (i    , j + 1);
      unsigned cR2 = cur_img (i + 1, j + 1);

      unsigned sr = ( cL0        & 0xFF) + ( cL1        & 0xFF) + ( cL2        & 0xFF)
                  + ( cM0        & 0xFF) + ( cM1        & 0xFF) + ( cM2        & 0xFF)
                  + ( cR0        & 0xFF) + ( cR1        & 0xFF) + ( cR2        & 0xFF);

      unsigned sg = ((cL0 >>  8) & 0xFF) + ((cL1 >>  8) & 0xFF) + ((cL2 >>  8) & 0xFF)
                  + ((cM0 >>  8) & 0xFF) + ((cM1 >>  8) & 0xFF) + ((cM2 >>  8) & 0xFF)
                  + ((cR0 >>  8) & 0xFF) + ((cR1 >>  8) & 0xFF) + ((cR2 >>  8) & 0xFF);

      unsigned sb = ((cL0 >> 16) & 0xFF) + ((cL1 >> 16) & 0xFF) + ((cL2 >> 16) & 0xFF)
                  + ((cM0 >> 16) & 0xFF) + ((cM1 >> 16) & 0xFF) + ((cM2 >> 16) & 0xFF)
                  + ((cR0 >> 16) & 0xFF) + ((cR1 >> 16) & 0xFF) + ((cR2 >> 16) & 0xFF);

      unsigned sa = ((cL0 >> 24) & 0xFF) + ((cL1 >> 24) & 0xFF) + ((cL2 >> 24) & 0xFF)
                  + ((cM0 >> 24) & 0xFF) + ((cM1 >> 24) & 0xFF) + ((cM2 >> 24) & 0xFF)
                  + ((cR0 >> 24) & 0xFF) + ((cR1 >> 24) & 0xFF) + ((cR2 >> 24) & 0xFF);

      next_img (i, j) = (sr / 9) | ((sg / 9) << 8) | ((sb / 9) << 16) | ((sa / 9) << 24);

      cL0 = cM0; cL1 = cM1; cL2 = cM2;
      cM0 = cR0; cM1 = cR1; cM2 = cR2;
    }
  }

  return 0;
}

// Sum the 3 pixels of one column, per channel
#define COL_SUM(col, sr, sg, sb, sa)                                           \
  do {                                                                         \
    unsigned t0 = cur_img (i - 1, (col));                                      \
    unsigned t1 = cur_img (i    , (col));                                      \
    unsigned t2 = cur_img (i + 1, (col));                                      \
    (sr) = ( t0        & 0xFF) + ( t1        & 0xFF) + ( t2        & 0xFF);    \
    (sg) = ((t0 >>  8) & 0xFF) + ((t1 >>  8) & 0xFF) + ((t2 >>  8) & 0xFF);    \
    (sb) = ((t0 >> 16) & 0xFF) + ((t1 >> 16) & 0xFF) + ((t2 >> 16) & 0xFF);    \
    (sa) = ((t0 >> 24) & 0xFF) + ((t1 >> 24) & 0xFF) + ((t2 >> 24) & 0xFF);    \
  } while (0)

// --- 2.7 optim6 : column reduction ---
// Both versions pinned to az4-a7900-2 so the numbers are comparable.
// -wt optim6, -O0 : 1213 / 1115 / 1115 ms
// -wt optim5, -O0 : 1426 / 1426 / 1425 ms
// 1.28x, taking 1115 against 1425; the 1213 was the first run, before the
// caches were warm. The sum of a column of 3 pixels is the same whichever
// pixel asks for it, so the rotation now carries the 4 column sums instead of
// the 9 pixel values. Per pixel and per channel that gives 2 additions for the
// new column plus 2 for the total, against 8 before. Hash still 19104192...
//
// Total at -O0: 5575.976 ms down to 1115 ms, 5.0x. The original code compiled
// at -O3 was 1036 ms, so by hand at -O0 I end up close to what gcc reaches on
// its own.
int blur_do_tile_optim6 (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++) {

    if (i == 0 || i == (int)DIM - 1) {
      for (int j = x; j < x + width; j++)
        blur_border_pixel (i, j);
      continue;
    }

    int j = x;

    if (j == 0) {
      blur_border_pixel (i, 0);
      j = 1;
    }

    unsigned lr, lg, lb, la;   // column sums, left
    unsigned mr, mg, mb, ma;   // middle
    unsigned rr, rg, rb, ra;   // right

    COL_SUM (j - 1, lr, lg, lb, la);
    COL_SUM (j    , mr, mg, mb, ma);

    for (; j < x + width; j++) {

      if (j == (int)DIM - 1) {
        blur_border_pixel (i, j);
        continue;
      }

      COL_SUM (j + 1, rr, rg, rb, ra);

      unsigned sr = (lr + mr + rr) / 9;
      unsigned sg = (lg + mg + rg) / 9;
      unsigned sb = (lb + mb + rb) / 9;
      unsigned sa = (la + ma + ra) / 9;

      next_img (i, j) = sr | (sg << 8) | (sb << 16) | (sa << 24);

      lr = mr; lg = mg; lb = mb; la = ma;
      mr = rr; mg = rg; mb = rb; ma = ra;
    }
  }

  return 0;
}

///////////////////////////// Sequential version (tiled)
// Suggested cmdline(s):
// ./run -l data/img/1024.png -k blur -v seq
//
unsigned blur_compute_seq (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {

    do_tile (0, 0, DIM, DIM);

    swap_images ();
  }

  return 0;
}

///////////////////////////// Tiled sequential version (tiled)
// Suggested cmdline(s):
// ./run -l data/img/1024.png -k blur -v tiled -ts 32 -m si
//
unsigned blur_compute_tiled (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {

    for (int y = 0; y < DIM; y += TILE_H)
      for (int x = 0; x < DIM; x += TILE_W)
        do_tile (x, y, TILE_W, TILE_H);

    swap_images ();
  }

  return 0;
}


// --- 2.8 All versions at every optimization level ---
// Dalek az4-mixed, node az4-a7900-2, img/1024.png, 100 iterations, variant seq.
// One run per cell, except the -O0 column which was measured earlier with
// 3 or 4 runs each.
//
//   tiling        -O0      -O1      -O2      -O3
//   default      5576     1572     1200     1240
//   default_nb   5312      971      938      720
//   optim1       5140      877      923      785
//   optim2       5155      806      752      752
//   optim3       2050      747      763      736
//   optim4       1335      694      637      654
//   optim5       1425      716      664      680
//   optim6       1115      519      518      490
//
// The fastest combination is optim6 at -O3, 490 ms. That is 11.4x against the
// untouched code at -O0 (5576 ms) and still 2.5x against the untouched code at
// -O3 (1240 ms), so the compiler contributes roughly 4.5x and the source
// transformations another 2.5x on top.
//
// What the table shows:
// - optim3 (hand inlining) only pays at -O0. From -O1 gcc inlines ezv_c2r and
//   the rest by itself and optim3 falls back into the noise around optim2.
// - optim2 (y unroll) does the opposite: useless at -O0, useful from -O1,
//   because the 9 pixel values then live in registers and not on the stack.
// - optim4 and optim6 pay at every level. They remove memory accesses by
//   reusing values from one pixel to the next, and no optimization level does
//   that transformation here.
// - optim5 costs 20 to 30 ms against optim4 at every level, 3 to 4%. That is
//   what the exact borders cost.
// - default is slower at -O3 than at -O2 (1240 against 1200) while default_nb
//   is much faster at -O3 than at -O2 (720 against 938). With one run per cell
//   I would not trust the first gap, but the second is too large to be noise.


// --- 2.9 [Bonus] Same code on the different Dalek CPUs ---
// img/1024.png, 100 iterations, variant seq, one run per cell.
//
//   partition    CPU                        -O0 default  -O0 optim6  -O3 default  -O3 optim6
//   az4-n4090    Ryzen 9 7945HX (Zen 4)          5432        1208        1239         526
//   az4-a7900    Ryzen 9 7945HX (Zen 4)          5475        1215        1238         523
//   iml-ia770    Core Ultra 9 185H               5337        1165         998         362
//   az5-a890m    Ryzen AI 9 HX 370 (Zen 5)       4634         898        1004         377
//
// The two az4 partitions hold the same CPU and agree to within 1%, which gives
// an idea of the measurement error.
// For this kernel the fastest core is the Core Ultra 9 185H at -O3 (362 ms),
// with the Zen 5 just behind (377 ms). At -O0 the Zen 5 is the fastest (898 ms).
// The Zen 4 is the slowest of the three on single core work here, about 1.4x
// behind at -O3, even though it is the desktop class chip of the cluster.
// Careful with these numbers: the Core Ultra 9 has P and E cores and the
// Ryzen AI 9 has Zen 5 and Zen 5c cores, and SLURM does not tell me which kind
// the job ran on, so with one run per cell this is a trend and not a ranking.
//
// Best result of the lab: optim6 at -O3 on iml-ia770, 362 ms, against 5432 ms
// for the original code at -O0 on az4-n4090, so 15x.