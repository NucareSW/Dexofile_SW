#ifndef INCLUDE_MODEL_SPECTRUM_H_
#define INCLUDE_MODEL_SPECTRUM_H_

#include "config.h"
#include "Types.h"
#include "util/nc_exception.h"

#include <stddef.h>
#include <array>
#include <stdexcept>
#include <math.h>
#include <QStringList>

template <class Data = float, size_t N = nucare::CHSIZE>
class Spectrum_t
{
   private:
    std::array<Data, N> m_data = {0};
    double m_acqTime = 0;
    double m_realTime = 0;
    double m_totalCount = 0;
    int m_fillCps = 0;
    int m_detID = -1;
    double m_count_rate = 0.0;

   public:
    using Channel  = Data;
    using SPC_DATA = Data*;

    Spectrum_t() noexcept : m_acqTime(1), m_totalCount(0), m_fillCps(0) {}

    explicit Spectrum_t(const Data* spc_data) noexcept(noexcept(setData(spc_data))) : Spectrum_t() {
        setData(spc_data);
    }

    Spectrum_t(const Spectrum_t& spc) = delete;

    Spectrum_t& operator=(const Spectrum_t& spc) = delete;

    ~Spectrum_t() = default;

   public:
    void update() {
        m_totalCount = m_fillCps;
        for (const auto& val : m_data) {
            m_totalCount += val;
        }
    }

    static inline constexpr size_t getSize() { return N; }

    void setAcqTime(double acqTime) noexcept { m_acqTime = acqTime; }

    void setRealTime(double realTime) noexcept { m_realTime = realTime; }

    void setData(const Data* data_ptr) {
        if (data_ptr) {
            std::copy(data_ptr, data_ptr + N, m_data.begin());
            update();
        }
    }

    void setDetectorID(int id) noexcept { m_detID = id; }

    void setCountRate(double count_rate) noexcept { m_count_rate = count_rate; }

    Data* data() noexcept { return m_data.data(); }

    const Data* dataConst() const noexcept { return m_data.data(); }

    double getTotalCount() const noexcept { return m_totalCount; }

    double getAvgCps() const {
        if (m_acqTime > 0) {
            return m_totalCount / m_acqTime;
        }
        return 0.0;
    }

    void setFillCps(int cps) noexcept { m_fillCps = cps; }

    int getFillCps() const noexcept { return m_fillCps; }

    double getAcqTime() const noexcept { return m_acqTime; }

    double getRealTime() const noexcept { return m_realTime; }

    int getDetectorID() const noexcept { return m_detID; }

    double getCountRate() const noexcept { return m_count_rate; }

    void reset() {
        std::fill(m_data.begin(), m_data.end(), 0);
        m_acqTime = 1;
        m_realTime = 0;
        m_totalCount = 0;
        m_fillCps = 0;
        m_detID = -1;
        m_count_rate = 0.0;
    }

    void accumulate(const Spectrum_t<Data, N>& spc) {
        for (size_t i = 0; i < N; ++i) {
            m_data[i] += spc.m_data[i];
        }
        m_acqTime += spc.m_acqTime;
        m_realTime += spc.m_realTime;
        m_fillCps += spc.m_fillCps;
        update();
    }

    auto begin() { return m_data.begin(); }
    auto end() { return m_data.end(); }

    Spectrum_t<Data, N> operator+(const Spectrum_t<Data, N>& spc) const {
        Spectrum_t<Data, N> result = *this;
        result.accumulate(spc);
        return result;
    }

    Data& operator[](const unsigned int i) {
        if (i >= N) {
            throw std::out_of_range("Spectrum_t::operator[]: index out of bounds");
        }
        return m_data[i];
    }

    const Data& operator[](const unsigned int i) const {
        if (i >= N) {
            throw std::out_of_range("Spectrum_t::operator[] const: index out of bounds");
        }
        return m_data[i];
    }

    const Data& at(const unsigned int i) const {
        return m_data.at(i);
    }

    QString toString() const {
        QStringList parts;
        parts.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            parts.append(QString::number(m_data[i]));
        }
        return parts.join(",");
    }

    static Spectrum_t* pFromString(const QString& s) {
        QStringList dataParts = s.split(',');
        if (dataParts.size() != getSize()) {
            auto msg = QString::asprintf("Spectrum size %d is not same with %d", getSize(), dataParts.size());
            NC_THROW_ARG_ERROR(msg);
        }

        Spectrum_t* ret = new Spectrum_t();
        for (int i = 0; i < ret->getSize(); i++) {
            ret->m_data[i] = dataParts[i].toDouble();
        }

        return ret;
    }

    template<class O>
//    using O = Spectrum_t;
    static void convertSpectrum(Spectrum_t& in, O& out, const double ratio) {
        if(ratio > 1) {
            bool First = true;
            double sum1 = 0, sum2 = 0, z_flt = 0, z_flt_pre = 0, ztmp = 0;
            int ind_chn = 0;

            int z_int = 0;
            for (size_t i = 0; i < out.getSize(); i++) {
                out[i] = 0;

                if (ratio < 1) {
                    sum1 = sum1 + ratio;

                    if (sum1 > 1) {
                        ind_chn = ind_chn + 1;
                        sum2 = sum1 - 1;
                        out[i] = (ratio - sum2) * in[ind_chn - 1] + sum2 * in[ind_chn];
                        sum1 = sum2;
                    } else {
                        sum2 = ratio;
                        out[i] = sum2 * in[ind_chn];
                    }

                } else if (ratio > 1) {
                    if (First == true) {
                        First = false;
                        z_int = (int) floor(ratio);
                        z_flt = ratio - z_int;

                        if (ind_chn + z_int + 1 > in.getSize() - 1) {
                            break;
                        }

                        sum1 = 0;
                        for (int j = 1; j <= z_int; j++) {
                            sum1 = sum1 + in[ind_chn + j];
                        }

                        sum1 = sum1 + z_flt * in[ind_chn + z_int + 1];

                        ind_chn = ind_chn + z_int + 1;
                        z_flt_pre = 1 - z_flt;
                    } else {
                        ztmp = ratio - z_flt_pre;

                        z_int = (int) floor(ztmp);

                        if (z_int >= 1) {
                            z_flt = ztmp - z_int;

                            if (ind_chn + z_int + 1 > in.getSize() - 1) {
                                break;
                            }

                            sum1 = z_flt_pre * in[ind_chn];

                            for (int j = 1; j <= z_int; j++) {
                                sum1 = sum1 + in[ind_chn + j];
                            }

                            sum1 = sum1 + z_flt * in[ind_chn + z_int + 1];

                            ind_chn = ind_chn + z_int + 1;
                            z_flt_pre = 1 - z_flt;

                        } else {
                            z_flt = ratio - z_flt_pre;

                            if (ind_chn + z_int + 1 > in.getSize() - 1) {
                                break;
                            }

                            sum1 = z_flt_pre * in[ind_chn] + z_flt * in[ind_chn + 1];
                            z_flt_pre = 1 - z_flt;
                            ind_chn = ind_chn + 1;
                        }
                    }
                    out[i] = sum1;
                } else if (ratio == 1) {
                    if (ind_chn < in.getSize()) {
                        out[i] = in[ind_chn];
                        ind_chn = ind_chn + 1;
                    }

                }
            }
        }
        else
        {
            if(out.getSize() < in.getSize())
            {
                for (int i = 0; i < out.getSize();i++)
                {
                    out[i] = in[i];
                }
            }

        }
    }

    static auto channelToEnergy(const Channel channel, const Coeffcients &params) {
        return POLY(channel, params[0], params[1], params[2]);
    }

    static auto energyToFWHM(const Energy energy, const FWHM &fwhm)
    {
        return fwhm[0] * sqrt(energy) + fwhm[1];
    }

    static auto channelToFWHM(const Channel channel, const FWHM& fwhm, const Coeffcients& coeff)
    {
        Energy energy = std::max(channelToEnergy(channel, coeff), (Energy) 1.0);
        return energyToFWHM(energy, fwhm);
    }
};

typedef Spectrum_t<double, nucare::CHSIZE> Spectrum;
typedef Spectrum_t<double, nucare::HW_CHSIZE> HwSpectrum;
typedef Spectrum_t<double, nucare::BINSIZE> BinSpectrum;

#endif /* INCLUDE_MODEL_SPECTRUM_H_ */
