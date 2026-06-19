/* * gamma_ndt.c  —  Implementation
 */
#include "gamma_ndt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

/* ── Default parameters (fitted from calibration data) ────── */
const GammaModel GAMMA_DEFAULT = {
    .N0  = 287.1098,   /* air NC2 rate  (172553.0 counts / 601s) [CPS] */
    .mu1 = 0.03589,    /* mm-1  (0.3589 cm-1)                          */
    .S   = 57.7433,    /* scatter sat.  (34703.7 counts  / 601s) [CPS] */
    .mus = 0.07231,    /* mm-1  (scatter sat. ~1/mus=13.8mm)            */
};

/* ═══════════════════════════════════════════════════════════
 * Forward model
 * ═══════════════════════════════════════════════════════════ */
double gamma_nc2_predict(double x_mm, const GammaModel *m)
{
    return m->N0 * exp(-m->mu1 * x_mm)
         + m->S  * (1.0 - exp(-m->mus * x_mm));
}

/* ═══════════════════════════════════════════════════════════
 * Brent's method  —  find root of f(x, ctx) = 0 in [a, b]
 * Returns root; sets *err on failure.
 * ═══════════════════════════════════════════════════════════ */
#define BRENT_MAXITER 128
#define BRENT_EPS     2.2e-16

typedef struct { const GammaModel *m; double nc2_target; } BrentCtx;

static double brent_f(double x, const BrentCtx *c)
{
    return gamma_nc2_predict(x, c->m) - c->nc2_target;
}

static double brent_solve(const BrentCtx *ctx,
                           double a, double b,
                           double tol, int *err)
{
    double fa = brent_f(a, ctx);
    double fb = brent_f(b, ctx);

    if (fa * fb > 0.0) { *err = GAMMA_ERR_BRACKET; return 0.0; }

    double c = a, fc = fa, d = b - a, e = d;
    *err = GAMMA_OK;

    for (int i = 0; i < BRENT_MAXITER; i++) {
        if (fb * fc > 0.0) { c = a; fc = fa; d = e = b - a; }
        if (fabs(fc) < fabs(fb)) {
            a = b; fa = fb;
            b = c; fb = fc;
            c = a; fc = fa;
        }
        double tol1 = 2.0 * BRENT_EPS * fabs(b) + 0.5 * tol;
        double xm   = 0.5 * (c - b);
        if (fabs(xm) <= tol1 || fabs(fb) < DBL_EPSILON) return b;

        double s, p, q, r;
        if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
            s = fb / fa;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                q = fa / fc; r = fb / fc;
                p = s * (2.0*xm*q*(q - r) - (b - a)*(r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q; else p = -p;
            s = e; e = d;
            if (2.0*p < 3.0*xm*q - fabs(tol1*q) && p < fabs(0.5*s*q))
                d = p / q;
            else
                { d = xm; e = d; }
        } else {
            d = xm; e = d;
        }
        a = b; fa = fb;
        if (fabs(d) > tol1) b += d;
        else                 b += (xm > 0.0 ? tol1 : -tol1);
        fb = brent_f(b, ctx);
    }
    *err = GAMMA_ERR_ITER;
    return b;
}

/* ═══════════════════════════════════════════════════════════
 * Inverse: NC2 → thickness
 * ═══════════════════════════════════════════════════════════ */
int gamma_thickness(double nc2, const GammaModel *m, double *out_mm)
{
    if (nc2 <= 0.0)    { *out_mm = -1.0; return GAMMA_ERR_INPUT; }
    if (nc2 >= m->N0)  { *out_mm =  0.0; return GAMMA_OK; }

    /* Expand upper bracket until model(x_hi) < nc2 */
    double x_hi = 1.0;
    while (gamma_nc2_predict(x_hi, m) > nc2) {
        x_hi *= 2.0;
        if (x_hi > 2000.0) { *out_mm = -1.0; return GAMMA_ERR_BRACKET; }
    }

    BrentCtx ctx = { m, nc2 };
    int err = GAMMA_OK;
    *out_mm = brent_solve(&ctx, 0.0, x_hi, 1e-6 /* mm */, &err);
    return err;
}

/* ═══════════════════════════════════════════════════════════
 * Nelder-Mead simplex optimizer (3-D)
 *   Minimizes sum of squared thickness errors given fixed N0.
 * ═══════════════════════════════════════════════════════════ */
#define NM_DIM     3      /* mu1, S, mus              */
#define NM_MAXITER 5000
#define NM_FTOL    1e-10

typedef struct {
    const CalibPoint *pts;
    int n_pts;
    const GammaModel *m_base;  /* N0 comes from here */
} NMCtx;

/* Objective: sum of squared thickness errors */
static double nm_obj(const double p[NM_DIM], const NMCtx *ctx)
{
    GammaModel m = *ctx->m_base;
    m.mu1 = p[0]; m.S = p[1]; m.mus = p[2];

    /* Penalize physically impossible parameters */
    if (m.mu1 <= 0.0 || m.S < 0.0 || m.mus <= 0.0) return 1e30;

    double sse = 0.0;
    for (int i = 0; i < ctx->n_pts; i++) {
        double x_calc;
        if (gamma_thickness(ctx->pts[i].nc2, &m, &x_calc) != GAMMA_OK)
            return 1e30;
        double res = x_calc - ctx->pts[i].true_mm;
        sse += res * res;
    }
    return sse;
}

int gamma_calibrate(const CalibPoint *pts, int n_pts,
                    const GammaModel *m_in, GammaModel *m_out)
{
    if (n_pts < NM_DIM) return GAMMA_ERR_INPUT;

    NMCtx ctx = { pts, n_pts, m_in };

    /* Simplex: (n+1) vertices × n coords */
    double simp[NM_DIM+1][NM_DIM];
    double fval[NM_DIM+1];

    /* Initial vertex from m_in */
    simp[0][0] = m_in->mu1;
    simp[0][1] = m_in->S;
    simp[0][2] = m_in->mus;
    fval[0]    = nm_obj(simp[0], &ctx);

    /* Remaining vertices: perturb each coord by 5% */
    double scale[NM_DIM] = { 0.05 * m_in->mu1,
                              0.05 * m_in->S,
                              0.05 * m_in->mus };
    for (int i = 1; i <= NM_DIM; i++) {
        memcpy(simp[i], simp[0], sizeof(simp[0]));
        simp[i][i-1] += (scale[i-1] > 0.0 ? scale[i-1] : 0.001);
        fval[i] = nm_obj(simp[i], &ctx);
    }

    double pbar[NM_DIM], prefl[NM_DIM], pexp[NM_DIM], pcon[NM_DIM];

    for (int iter = 0; iter < NM_MAXITER; iter++) {
        /* Sort: fval[0] ≤ fval[1] ≤ ... ≤ fval[N] */
        for (int i = 0; i <= NM_DIM; i++)
            for (int j = i+1; j <= NM_DIM; j++)
                if (fval[j] < fval[i]) {
                    double tmp = fval[i]; fval[i]=fval[j]; fval[j]=tmp;
                    for (int k=0;k<NM_DIM;k++){
                        tmp=simp[i][k]; simp[i][k]=simp[j][k]; simp[j][k]=tmp;
                    }
                }

        /* Convergence check */
        double range = fval[NM_DIM] - fval[0];
        if (range < NM_FTOL) break;

        /* Centroid of all but worst */
        for (int k=0;k<NM_DIM;k++) {
            pbar[k]=0.0;
            for(int i=0;i<NM_DIM;i++) pbar[k]+=simp[i][k];
            pbar[k]/=NM_DIM;
        }

        /* Reflection */
        for(int k=0;k<NM_DIM;k++) prefl[k]=2.0*pbar[k]-simp[NM_DIM][k];
        double frefl = nm_obj(prefl, &ctx);

        if (frefl < fval[0]) {
            /* Expansion */
            for(int k=0;k<NM_DIM;k++) pexp[k]=3.0*pbar[k]-2.0*simp[NM_DIM][k];
            double fexp = nm_obj(pexp, &ctx);
            if (fexp < frefl) { memcpy(simp[NM_DIM],pexp,sizeof(pexp)); fval[NM_DIM]=fexp; }
            else              { memcpy(simp[NM_DIM],prefl,sizeof(prefl)); fval[NM_DIM]=frefl; }
        } else if (frefl < fval[NM_DIM-1]) {
            memcpy(simp[NM_DIM], prefl, sizeof(prefl));
            fval[NM_DIM] = frefl;
        } else {
            /* Contraction */
            if (frefl < fval[NM_DIM])
                for(int k=0;k<NM_DIM;k++) pcon[k]=0.5*(pbar[k]+prefl[k]);
            else
                for(int k=0;k<NM_DIM;k++) pcon[k]=0.5*(pbar[k]+simp[NM_DIM][k]);
            double fcon = nm_obj(pcon, &ctx);
            if (fcon < fval[NM_DIM]) {
                memcpy(simp[NM_DIM], pcon, sizeof(pcon));
                fval[NM_DIM] = fcon;
            } else {
                /* Shrink */
                for(int i=1;i<=NM_DIM;i++) {
                    for(int k=0;k<NM_DIM;k++)
                        simp[i][k]=0.5*(simp[0][k]+simp[i][k]);
                    fval[i]=nm_obj(simp[i],&ctx);
                }
            }
        }
    }

    *m_out    = *m_in;
    m_out->mu1 = simp[0][0];
    m_out->S   = simp[0][1];
    m_out->mus = simp[0][2];
    return GAMMA_OK;
}

/* ═══════════════════════════════════════════════════════════
 * Update air reference N0 (running mean)
 * ═══════════════════════════════════════════════════════════ */
void gamma_update_n0(GammaModel *m, double nc2_air, int n_samples)
{
    /* Incremental mean: mean_n = mean_{n-1} + (x_n - mean_{n-1}) / n */
    if (n_samples <= 0) n_samples = 1;
    m->N0 += (nc2_air - m->N0) / (double)n_samples;
}

/* ═══════════════════════════════════════════════════════════
 * Validate: print residuals at calibration points
 * ═══════════════════════════════════════════════════════════ */
void gamma_validate(const GammaModel *m,
                    const CalibPoint *pts, int n_pts)
{
    printf("  mu1=%.5f mm-1  S=%.1f  mus=%.5f mm-1  N0=%.1f\n",
           m->mu1, m->S, m->mus, m->N0);
    printf("  %-8s  %-10s  %-10s  %-10s  %s\n",
           "True(mm)", "NC2_meas", "NC2_pred", "x_calc(mm)", "Err%");

    /* Air point */
    double nc2_air_pred = gamma_nc2_predict(0.0, m);
    printf("  %-8.1f  %-10.1f  %-10.1f  %-10.3f  %+.2f%%\n",
           0.0, m->N0, nc2_air_pred, 0.0, 0.0);

    for (int i = 0; i < n_pts; i++) {
        double nc2_pred = gamma_nc2_predict(pts[i].true_mm, m);
        double x_calc;
        gamma_thickness(pts[i].nc2, m, &x_calc);
        double err = (pts[i].true_mm > 0.0)
                   ? (x_calc - pts[i].true_mm) / pts[i].true_mm * 100.0
                   : x_calc;
        printf("  %-8.1f  %-10.1f  %-10.1f  %-10.3f  %+.2f%%\n",
               pts[i].true_mm, pts[i].nc2, nc2_pred, x_calc, err);
    }
}
