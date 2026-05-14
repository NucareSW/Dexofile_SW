#include "ndt_util.h"
#include "util/nc_exception.h"
#include "config.h"

#include <cmath>
using namespace nucare;

double ndt::Mass_Attenuation_coefficient_Iron(const Energy En, const Material material) {
    // Mass attenuation coefficient data (cm2/g)
    std::vector<Energy> mass_abs_coef;

    // Energy data in MeV
    std::vector<Energy> Mev;

    switch (material) {
    case IRON:
        mass_abs_coef = {
            9.09E+03, 3.40E+03, 1.63E+03, 5.58E+02, 2.57E+02, 1.40E+02, 8.48E+01,
            5.32E+01, 4.08E+02, 3.06E+02, 1.71E+02, 5.71E+01, 2.57E+01, 8.18E+00,
            3.63E+00, 1.96E+00, 1.21E+00, 5.95E-01, 3.72E-01, 1.96E-01, 1.46E-01,
            1.10E-01, 9.40E-02, 8.41E-02, 7.70E-02, 6.70E-02, 6.00E-02, 5.35E-02,
            4.88E-02, 4.27E-02, 3.62E-02, 3.31E-02, 3.15E-02, 3.06E-02, 2.99E-02,
            2.99E-02, 3.09E-02, 3.22E-02
        };
        Mev = {
                1.00E-03, 1.50E-03, 2.00E-03, 3.00E-03, 4.00E-03, 5.00E-03, 6.00E-03,
                7.11E-03, 7.11E-03, 8.00E-03, 1.00E-02, 1.50E-02, 2.00E-02, 3.00E-02,
                4.00E-02, 5.00E-02, 6.00E-02, 8.00E-02, 1.00E-01, 1.50E-01, 2.00E-01,
                3.00E-01, 4.00E-01, 5.00E-01, 6.00E-01, 8.00E-01, 1.00E+00, 1.25E+00,
                1.50E+00, 2.00E+00, 3.00E+00, 4.00E+00, 5.00E+00, 6.00E+00, 8.00E+00,
                1.00E+01, 1.50E+01, 2.00E+01
            };
        break;
    case ALUMINUM:
        mass_abs_coef = {
            1.19E+03, 4.02E+02, 3.62E+02, 3.96E+03, 2.26E+03, 7.88E+02, 3.61E+02,
            1.93E+02, 1.15E+02, 5.03E+01, 2.62E+01, 7.96E+00, 3.44E+00, 1.13E+00,
            5.69E-01, 3.68E-01, 2.78E-01, 2.02E-01, 1.70E-01, 1.38E-01, 1.22E-01,
            1.04E-01, 9.28E-02, 8.45E-02, 7.80E-02, 6.84E-02, 6.15E-02, 5.50E-02,
            5.01E-02, 4.32E-02, 3.54E-02, 3.11E-02, 2.84E-02, 2.66E-02, 2.44E-02,
            2.32E-02, 2.20E-02, 2.17E-02
        };
        Mev = {
            1.00E-03, 1.50E-03, 1.56E-03, 1.56E-03, 2.00E-03, 3.00E-03, 4.00E-03,
            5.00E-03, 6.00E-03, 8.00E-03, 1.00E-02, 1.50E-02, 2.00E-02, 3.00E-02,
            4.00E-02, 5.00E-02, 6.00E-02, 8.00E-02, 1.00E-01, 1.50E-01, 2.00E-01,
            3.00E-01, 4.00E-01, 5.00E-01, 6.00E-01, 8.00E-01, 1.00E+00, 1.25E+00,
            1.50E+00, 2.00E+00, 3.00E+00, 4.00E+00, 5.00E+00, 6.00E+00, 8.00E+00,
            1.00E+01, 1.50E+01, 2.00E+01
        };
        break;
    }

    // Convert energy data from MeV to KeV
    std::vector<double> KeV(Mev.size());
    for (size_t i = 0; i < Mev.size(); ++i) {
        KeV[i] = Mev[i] * 1000.0;
    }

    // Find the insertion point for En using binary search
    auto it = std::lower_bound(KeV.begin(), KeV.end(), En);
    size_t ind = std::distance(KeV.begin(), it);

    // Handle edge cases
    if (ind == 0) {
        if (En < KeV[0]) {
             NC_THROW_ALG_ERROR("Energy (En) is below the lowest data point.");
        }
        return mass_abs_coef[0]; // En == KeV[0]
    }
    if (ind == KeV.size()) {
        NC_THROW_ALG_ERROR("Energy (En) is above the highest data point.");
    }

    // If En matches an exact data point
    if (En == KeV[ind]) {
        return mass_abs_coef[ind];
    }

    // Perform linear interpolation
    Energy x1 = KeV[ind - 1];
    Energy x2 = KeV[ind];
    Energy y1 = mass_abs_coef[ind - 1];
    Energy y2 = mass_abs_coef[ind];

    Energy a = (y1 - y2) / (x1 - x2); // Slope
    Energy b = y1 - a * x1;           // Intercept

    return a * En + b;
}

/**
 * @brief Estimates tc_est iteratively based on given parameters and mu values.
 *
 * This function performs an iterative calculation to refine the estimate of tc_est.
 * It uses a linear update rule derived from the provided input parameters.
 *
 * @param A A double value used in the numerator calculation.
 * @param B A double value used in the numerator calculation.
 * @param params A std::vector<double> containing polynomial coefficients (a1, b1, c1).
 * Expected to have at least 3 elements.
 * @param mu_ce1 A double value for mu_ce1.
 * @param mu_ce2 A double value for mu_ce2.
 * @param mu_pe1 A double value for mu_pe1.
 * @param mu_pe2 A double value for mu_pe2.
 * @param tp A double value for tp.
 * @param tc_est_initial The initial estimate for tc_est. It will be updated iteratively.
 * @return The refined estimate of tc_est after iteration, or the initial value if conditions are met.
 */
double ndt::estimate_tc_from_Est_E2(double A, double B, const std::vector<double>& params,
                               double mu_ce1, double mu_ce2,
                               double mu_pe1, double mu_pe2,
                               double tp, double tc_est_initial) {

    double tc_est = tc_est_initial;

    // Ensure tc_est is not negative (equivalent to MATLAB's if(tc_est<0) tc_est=0;)
    if (tc_est < 0) {
        tc_est = 0;
    }

    const int max_iter = 10000;
    const double tol = 1e-6; // Tolerance for convergence

    // Extract polynomial coefficients from params vector
    // Ensure params has enough elements to avoid out-of-bounds access
    // This assumes params has at least 3 elements for a1, b1, c1
    if (params.size() < 3) {
        NC_THROW_ALG_ERROR("Invalid parameter");
    }
    double a1 = params[0];
    double b1 = params[1];
    double c1 = params[2];

    // a2, b2, c2 are commented out in MATLAB and k2 is hardcoded to 1.
    // double a2 = (params.size() > 3) ? params[3] : 0.0;
    // double b2 = (params.size() > 4) ? params[4] : 0.0;
    // double c2 = (params.size() > 5) ? params[5] : 0.0;

    for (int iter = 0; iter < max_iter; ++iter) {
        double k1 = POLY(tc_est, a1, b1, c1);
        // k2 is hardcoded to 1 in your MATLAB code
        double k2 = 1.0; // k2 = a2 * std::pow(tc_est, 2) + b2 * tc_est + c2;

        double numerator = mu_ce1 * (k1 * A - mu_pe1 * tp) + mu_ce2 * (k2 * B - mu_pe2 * tp);
        double denominator = std::pow(mu_ce1, 2) + std::pow(mu_ce2, 2);

        // Avoid division by zero if denominator is too small
        if (std::abs(denominator) < 1e-12) { // Use a small epsilon to check for near-zero
            // Handle error: denominator is zero or near zero.
            // This might indicate an issue with the input values or convergence.
            // For now, break or return the current tc_est.
            return tc_est;
        }

        double tc_new = numerator / denominator;

        if (std::abs(tc_new - tc_est) < tol) {
            break; // Converged
        }

        tc_est = tc_new; // Update estimate for next iteration
    }

    return tc_est; // Return the final estimated value
}
