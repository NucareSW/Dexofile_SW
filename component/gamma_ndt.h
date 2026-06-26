/*
 * gamma_ndt.h  —  Gamma-ray NDT thickness measurement
 *
 * Model (3-parameter scatter buildup):
 *   NC2(x) = N0 · exp(-mu1 · x)  +  S · (1 - exp(-mus · x))
 *   └──────────────────────────┘    └───────────────────────┘
 *      Beer-Lambert direct beam          Scatter from sample
 *
 *   x   = thickness [mm]
 *   N0  = reference count at x=0 (air, no sample)
 *   mu1 = primary attenuation coefficient [mm⁻¹]
 *   S   = scatter saturation level [counts]
 *   mus = scatter buildup rate [mm⁻¹]
 *
 * At x=0: NC2 = N0  →  x_measured = 0 exactly
 */
#ifndef GAMMA_NDT_H
#define GAMMA_NDT_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Source decay ─────────────────────────────────────────── */
#define BA133_T_HALF_DAYS  3837.0   /* Ba-133 T½ = 10.51 years */

/* ── Return codes ──────────────────────────────────────────── */
#define GAMMA_OK           0
#define GAMMA_ERR_BRACKET -1   /* could not bracket root      */
#define GAMMA_ERR_ITER    -2   /* max iterations reached      */
#define GAMMA_ERR_INPUT   -3   /* invalid input (nc2 <= 0)    */

/* ── Model parameters ─────────────────────────────────────── */
typedef struct {
    double N0;    /* air reference count              [counts] */
    double mu1;   /* primary attenuation coefficient  [mm⁻¹]  */
    double S;     /* scatter saturation               [counts] */
    double mus;   /* scatter buildup rate             [mm⁻¹]  */
} GammaModel;

/* ── Calibration data point ──────────────────────────────── */
typedef struct {
    double true_mm;   /* known true thickness  [mm]     */
    double nc2;       /* measured NetCount (356 keV)    */
} CalibPoint;

/* ── Default fitted parameters (Ba-133 / 356 keV, steel) ── */
/* N0 and S are in CPS (counts/s); mu1, mus in mm⁻¹         */
extern const GammaModel GAMMA_DEFAULT;

/* ── API ──────────────────────────────────────────────────── */

/*
 * Forward model: thickness → predicted NC2.
 * Always returns a positive value; x must be >= 0.
 */
double gamma_nc2_predict(double x_mm, const GammaModel *m);

/*
 * Inverse: measured NC2 → thickness [mm].
 *   nc2     : measured net count for 356 keV peak
 *   m       : model parameters
 *   out_mm  : output thickness in mm
 *   returns : GAMMA_OK or error code
 */
int gamma_thickness(double nc2, const GammaModel *m, double *out_mm);

/*
 * Fit model parameters (mu1, S, mus) from calibration data.
 * N0 is taken from the model as the fixed air reference.
 * Uses Nelder-Mead simplex minimizing sum of squared thickness errors.
 *   pts    : array of CalibPoint (must NOT include x=0 air point)
 *   n_pts  : number of calibration points
 *   m_in   : initial guess (N0 must be set to air NC2 mean)
 *   m_out  : fitted model
 *   returns: GAMMA_OK or error code
 */
int gamma_calibrate(const CalibPoint *pts, int n_pts,
                    const GammaModel *m_in, GammaModel *m_out);

/*
 * Update air reference (N0) from a running mean of air measurements.
 * Call this when measuring without sample; updates m->N0 in place.
 *   m          : model to update
 *   nc2_air    : new air NC2 measurement
 *   n_samples  : total samples seen so far (for running mean weight)
 */
void gamma_update_n0(GammaModel *m, double nc2_air, int n_samples);

/* Validate fitted model: print errors at each calibration point */
void gamma_validate(const GammaModel *m,
                    const CalibPoint *pts, int n_pts);

/*
 * Decay correction — trả về model MỚI với N0 và S đã scale theo decay.
 * mu1 và mus KHÔNG thay đổi (thuộc tính vật liệu, không phụ thuộc nguồn).
 *
 *   gamma_apply_decay()       — truyền số ngày kể từ lúc hiệu chỉnh
 *   gamma_apply_decay_epoch() — truyền Unix timestamp ngày hiệu chỉnh,
 *                               hàm tự lấy ngày hôm nay bằng time(NULL)
 *
 * Ví dụ:
 *   // 8 ngày sau hiệu chỉnh:
 *   GammaModel m = gamma_apply_decay(&GAMMA_DEFAULT, 8.0, BA133_T_HALF_DAYS);
 *
 *   // Dùng epoch ngày hiệu chỉnh (2026-06-19):
 *   struct tm t = {0};
 *   t.tm_year=126; t.tm_mon=5; t.tm_mday=19;
 *   GammaModel m = gamma_apply_decay_epoch(&GAMMA_DEFAULT,
 *                                          mktime(&t), BA133_T_HALF_DAYS);
 */
GammaModel gamma_apply_decay(const GammaModel *m,
                              double days_since_calib,
                              double t_half_days);

GammaModel gamma_apply_decay_epoch(const GammaModel *m,
                                   time_t calib_epoch,
                                   double t_half_days);

#ifdef __cplusplus
}
#endif
#endif /* GAMMA_NDT_H */
