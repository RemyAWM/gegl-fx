#define LAPLACE_RADIUS 2
#define EPSILON        1e-5f

void minmax(float x1, float x2, float x3,
            float x4, float x5,
            float *min_result,
            float *max_result)
{
    float min1, min2, max1, max2;

    if (x1 > x2)
    {
        max1 = x1;
        min1 = x2;
    }
    else
    {
        max1 = x2;
        min1 = x1;
    }

    if (x3 > x4)
    {
        max2 = x3;
        min2 = x4;
    }
    else
    {
        max2 = x4;
        min2 = x3;
    }

    if (min1 < min2)
        *min_result = fmin(min1, x5);
    else
        *min_result = fmin(min2, x5);
    if (max1 > max2)
        *max_result = fmax(max1, x5);
    else
        *max_result = fmax(max2, x5);
}

float4 get_pix(global float4 *in, int x, int y, int rowstride)
{
    int idx = x + y * rowstride;
    return in[idx];
}

kernel void pre_edgelaplace (global float4 *in,
                             global float4 *out)
{
    int gidx = get_global_id(0);
    int gidy = get_global_id(1);

    int src_width  = get_global_size(0) + 2;
    int src_height = get_global_size(1) + 2;

    int i = gidx + 1, j = gidy + 1;

    float4 cur_pix;

    cur_pix = get_pix(in, i - 1, j - 1, src_width);
    float pix_fl[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 0, j - 1, src_width);
    float pix_fm[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i + 1, j - 1, src_width);
    float pix_fr[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 1, j - 0, src_width);
    float pix_ml[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 0, j - 0, src_width);
    float pix_mm[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i + 1, j - 0, src_width);
    float pix_mr[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 1, j + 1, src_width);
    float pix_bl[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 0, j + 1, src_width);
    float pix_bm[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i + 1, j + 1, src_width);
    float pix_br[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    int c;
    float minval, maxval;
    float gradient[4];

    for (c = 0;c < 3; ++c)
    {
        minmax(pix_fm[c], pix_bm[c], pix_ml[c], pix_mr[c],
            pix_mm[c], &minval, &maxval);
        gradient[c] = 0.5f *
            fmax((maxval - pix_mm[c]),(pix_mm[c] - minval));
        gradient[c] =
            (pix_fl[c] + pix_fm[c] + pix_fr[c] +
             pix_bm[c] - 8.0f * pix_mm[c] + pix_br[c] +
             pix_ml[c] + pix_mr[c] + pix_bl[c]) <
             EPSILON ? -1.0f * gradient[c] : gradient[c];
    }
    gradient[3] = pix_mm[3];

    out[gidx + gidy * get_global_size(0)] = (float4)
        (gradient[0], gradient[1], gradient[2], gradient[3]);
}

kernel void knl_edgelaplace (global float4 *in,
                             global float4 *out)
{
    int gidx = get_global_id(0);
    int gidy = get_global_id(1);

    int src_width  = get_global_size(0) + 2;
    int src_height = get_global_size(1) + 2;

    int i = gidx + 1, j = gidy + 1;

    float4 cur_pix;

    cur_pix = get_pix(in, i - 1, j - 1, src_width);
    float pix_fl[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 0, j - 1, src_width);
    float pix_fm[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i + 1, j - 1, src_width);
    float pix_fr[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 1, j - 0, src_width);
    float pix_ml[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 0, j - 0, src_width);
    float pix_mm[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i + 1, j - 0, src_width);
    float pix_mr[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 1, j + 1, src_width);
    float pix_bl[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i - 0, j + 1, src_width);
    float pix_bm[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    cur_pix = get_pix(in, i + 1, j + 1, src_width);
    float pix_br[4] = {cur_pix.x, cur_pix.y, cur_pix.z, cur_pix.w};

    int c;
    float value[4];

    for (c = 0;c < 3; ++c)
    {
        float current = pix_mm[c];
        current =
            ((current > 0.0f) &&
             (pix_fl[c] < 0.0f || pix_fm[c] < 0.0f ||
              pix_fr[c] < 0.0f || pix_ml[c] < 0.0f ||
              pix_mr[c] < 0.0f || pix_bl[c] < 0.0f ||
              pix_bm[c] < 0.0f || pix_br[c] < 0.0f )
            ) ? current : 0.0f;
        value[c] = current;
    }
    value[3] = pix_mm[3];

    out[gidx + gidy * get_global_size(0)] = (float4)
        (value[0], value[1], value[2], value[3]);
}
