/*!
  \file SiStripNoises_PayloadInspector
  \Payload Inspector Plugin for SiStrip Noises
  \author M. Musich
  \version $Revision: 1.0 $
  \date $Date: 2017/09/21 13:59:56 $
*/

#include "CondCore/Utilities/interface/PayloadInspectorModule.h"
#include "CondCore/Utilities/interface/PayloadInspector.h"
#include "CondCore/CondDB/interface/Time.h"

// the data format of the condition to be inspected
#include "CondFormats/SiStripObjects/interface/SiStripNoises.h"
#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/SiStripDetId/interface/StripSubdetector.h"
#include "CondFormats/SiStripObjects/interface/SiStripDetSummary.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

// needed for the tracker map
#include "CommonTools/TrackerMap/interface/TrackerMap.h"

// auxilliary functions
#include "CondCore/SiStripPlugins/interface/SiStripPayloadInspectorHelper.h"
#include "CalibTracker/StandaloneTrackerTopology/interface/StandaloneTrackerTopology.h"

#include "CalibTracker/SiStripCommon/interface/SiStripDetInfoFileReader.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"

#include <memory>
#include <sstream>
#include <iostream>

// include ROOT
#include "TH2F.h"
#include "TF1.h"
#include "TGraphErrors.h"
#include "TLegend.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TStyle.h"
#include "TLatex.h"
#include "TPave.h"
#include "TPaveStats.h"
#include "TGaxis.h"

namespace {

  /************************************************
    test class
  *************************************************/

  class SiStripNoisesTest : public cond::payloadInspector::Histogram1D<SiStripNoises> {
  public:
    SiStripNoisesTest()
        : cond::payloadInspector::Histogram1D<SiStripNoises>("SiStrip Noise test", "SiStrip Noise test", 10, 0.0, 10.0),
          m_trackerTopo{StandaloneTrackerTopology::fromTrackerParametersXMLFile(
              edm::FileInPath("Geometry/TrackerCommonData/data/trackerParameters.xml").fullPath())} {
      Base::setSingleIov(true);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      for (auto const& iov : iovs) {
        std::shared_ptr<SiStripNoises> payload = Base::fetchPayload(std::get<1>(iov));
        if (payload.get()) {
          fillWithValue(1.);

          std::stringstream ss;
          ss << "Summary of strips noises:" << std::endl;

          //payload->printDebug(ss);
          payload->printSummary(ss, &m_trackerTopo);

          std::vector<uint32_t> detid;
          payload->getDetIds(detid);

          // for (const auto & d : detid) {
          //   int nstrip=0;
          //   SiStripNoises::Range range=payload->getRange(d);
          //   for( int it=0; it < (range.second-range.first)*8/9; ++it ){
          //     auto noise = payload->getNoise(it,range);
          //     nstrip++;
          //     ss << "DetId="<< d << " Strip=" << nstrip <<": "<< noise << std::endl;
          //   } // end of loop on strips
          // } // end of loop on detIds

          std::cout << ss.str() << std::endl;

        }  // payload
      }    // iovs
      return true;
    }  // fill
  private:
    TrackerTopology m_trackerTopo;
  };

  /************************************************
    1d histogram of SiStripNoises of 1 IOV 
  *************************************************/

  // inherit from one of the predefined plot class: Histogram1D
  class SiStripNoiseValue : public cond::payloadInspector::Histogram1D<SiStripNoises> {
  public:
    SiStripNoiseValue()
        : cond::payloadInspector::Histogram1D<SiStripNoises>(
              "SiStrip Noise values", "SiStrip Noise values", 100, 0.0, 10.0) {
      Base::setSingleIov(true);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      for (auto const& iov : iovs) {
        std::shared_ptr<SiStripNoises> payload = Base::fetchPayload(std::get<1>(iov));
        if (payload.get()) {
          std::vector<uint32_t> detid;
          payload->getDetIds(detid);

          for (const auto& d : detid) {
            SiStripNoises::Range range = payload->getRange(d);
            for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
              auto noise = payload->getNoise(it, range);
              //to be used to fill the histogram
              fillWithValue(noise);
            }  // loop over APVs
          }    // loop over detIds
        }      // payload
      }        // iovs
      return true;
    }  // fill
  };

  /************************************************
    templated 1d histogram of SiStripNoises of 1 IOV
  *************************************************/

  // inherit from one of the predefined plot class: PlotImage
  template <SiStripPI::OpMode op_mode_>
  class SiStripNoiseDistribution : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseDistribution() : cond::payloadInspector::PlotImage<SiStripNoises>("SiStrip Noise values") {
      setSingleIov(true);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      auto iov = iovs.front();

      TGaxis::SetMaxDigits(3);
      gStyle->SetOptStat("emr");

      std::shared_ptr<SiStripNoises> payload = fetchPayload(std::get<1>(iov));

      auto mon1D = std::unique_ptr<SiStripPI::Monitor1D>(new SiStripPI::Monitor1D(
          op_mode_,
          "Noise",
          Form("#LT Strip Noise #GT per %s for IOV [%s];#LTStrip Noise per %s#GT [ADC counts];n. %ss",
               opType(op_mode_).c_str(),
               std::to_string(std::get<0>(iov)).c_str(),
               opType(op_mode_).c_str(),
               opType(op_mode_).c_str()),
          100,
          0.1,
          10.));

      unsigned int prev_det = 0, prev_apv = 0;
      SiStripPI::Entry enoise;

      std::vector<uint32_t> detids;
      payload->getDetIds(detids);

      // loop on payload

      for (const auto& d : detids) {
        SiStripNoises::Range range = payload->getRange(d);

        unsigned int istrip = 0;

        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          auto noise = payload->getNoise(it, range);
          bool flush = false;
          switch (op_mode_) {
            case (SiStripPI::APV_BASED):
              flush = (prev_det != 0 && prev_apv != istrip / 128);
              break;
            case (SiStripPI::MODULE_BASED):
              flush = (prev_det != 0 && prev_det != d);
              break;
            case (SiStripPI::STRIP_BASED):
              flush = (istrip != 0);
              break;
          }

          if (flush) {
            mon1D->Fill(prev_apv, prev_det, enoise.mean());
            enoise.reset();
          }

          enoise.add(std::min<float>(noise, 30.5));
          prev_apv = istrip / 128;
          istrip++;
        }
        prev_det = d;
      }

      //=========================
      TCanvas canvas("Partion summary", "partition summary", 1200, 1000);
      canvas.cd();
      canvas.SetBottomMargin(0.11);
      canvas.SetTopMargin(0.07);
      canvas.SetLeftMargin(0.13);
      canvas.SetRightMargin(0.05);
      canvas.Modified();

      auto hist = mon1D->getHist();
      SiStripPI::makeNicePlotStyle(&hist);
      hist.SetStats(kTRUE);
      hist.SetFillColorAlpha(kRed, 0.35);
      hist.Draw();

      canvas.Update();

      TPaveStats* st = (TPaveStats*)hist.GetListOfFunctions()->FindObject("stats");
      st->SetLineColor(kRed);
      st->SetTextColor(kRed);
      st->SetX1NDC(.75);
      st->SetX2NDC(.95);
      st->SetY1NDC(.83);
      st->SetY2NDC(.93);

      TLegend legend = TLegend(0.13, 0.83, 0.43, 0.93);
      legend.SetHeader(Form("SiStrip Noise values per %s", opType(op_mode_).c_str()),
                       "C");  // option "C" allows to center the header
      legend.AddEntry(&hist, ("IOV: " + std::to_string(std::get<0>(iov))).c_str(), "F");
      legend.SetTextSize(0.025);
      legend.Draw("same");

      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());

      return true;
    }

    std::string opType(SiStripPI::OpMode mode) {
      std::string types[3] = {"Strip", "APV", "Module"};
      return types[mode];
    }
  };

  typedef SiStripNoiseDistribution<SiStripPI::STRIP_BASED> SiStripNoiseValuePerStrip;
  typedef SiStripNoiseDistribution<SiStripPI::APV_BASED> SiStripNoiseValuePerAPV;
  typedef SiStripNoiseDistribution<SiStripPI::MODULE_BASED> SiStripNoiseValuePerModule;

  /************************************************
  template 1d histogram comparison of SiStripNoises of 1 IOV
  *************************************************/

  // inherit from one of the predefined plot class: PlotImage
  template <SiStripPI::OpMode op_mode_>
  class SiStripNoiseDistributionComparisonBase : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseDistributionComparisonBase()
        : cond::payloadInspector::PlotImage<SiStripNoises>("SiStrip Noise values comparison") {}

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      std::vector<std::tuple<cond::Time_t, cond::Hash> > sorted_iovs = iovs;

      // make absolute sure the IOVs are sortd by since
      std::sort(begin(sorted_iovs), end(sorted_iovs), [](auto const& t1, auto const& t2) {
        return std::get<0>(t1) < std::get<0>(t2);
      });

      auto firstiov = sorted_iovs.front();
      auto lastiov = sorted_iovs.back();

      std::shared_ptr<SiStripNoises> f_payload = fetchPayload(std::get<1>(firstiov));
      std::shared_ptr<SiStripNoises> l_payload = fetchPayload(std::get<1>(lastiov));

      auto f_mon = std::unique_ptr<SiStripPI::Monitor1D>(new SiStripPI::Monitor1D(
          op_mode_,
          "f_Noise",
          Form("#LT Strip Noise #GT per %s for IOV [%s,%s];#LTStrip Noise per %s#GT [ADC counts];n. %ss",
               opType(op_mode_).c_str(),
               std::to_string(std::get<0>(firstiov)).c_str(),
               std::to_string(std::get<0>(lastiov)).c_str(),
               opType(op_mode_).c_str(),
               opType(op_mode_).c_str()),
          100,
          0.1,
          10.));

      auto l_mon = std::unique_ptr<SiStripPI::Monitor1D>(new SiStripPI::Monitor1D(
          op_mode_,
          "l_Noise",
          Form("#LT Strip Noise #GT per %s for IOV [%s,%s];#LTStrip Noise per %s#GT [ADC counts];n. %ss",
               opType(op_mode_).c_str(),
               std::to_string(std::get<0>(lastiov)).c_str(),
               std::to_string(std::get<0>(lastiov)).c_str(),
               opType(op_mode_).c_str(),
               opType(op_mode_).c_str()),
          100,
          0.1,
          10.));

      unsigned int prev_det = 0, prev_apv = 0;
      SiStripPI::Entry enoise;

      std::vector<uint32_t> f_detid;
      f_payload->getDetIds(f_detid);

      // loop on first payload
      for (const auto& d : f_detid) {
        SiStripNoises::Range range = f_payload->getRange(d);

        unsigned int istrip = 0;
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          float noise = f_payload->getNoise(it, range);
          //to be used to fill the histogram

          bool flush = false;
          switch (op_mode_) {
            case (SiStripPI::APV_BASED):
              flush = (prev_det != 0 && prev_apv != istrip / 128);
              break;
            case (SiStripPI::MODULE_BASED):
              flush = (prev_det != 0 && prev_det != d);
              break;
            case (SiStripPI::STRIP_BASED):
              flush = (istrip != 0);
              break;
          }

          if (flush) {
            f_mon->Fill(prev_apv, prev_det, enoise.mean());
            enoise.reset();
          }
          enoise.add(std::min<float>(noise, 30.5));
          prev_apv = istrip / 128;
          istrip++;
        }
        prev_det = d;
      }

      prev_det = 0, prev_apv = 0;
      enoise.reset();

      std::vector<uint32_t> l_detid;
      l_payload->getDetIds(l_detid);

      // loop on first payload

      for (const auto& d : l_detid) {
        SiStripNoises::Range range = l_payload->getRange(d);

        unsigned int istrip = 0;
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          float noise = l_payload->getNoise(it, range);

          bool flush = false;
          switch (op_mode_) {
            case (SiStripPI::APV_BASED):
              flush = (prev_det != 0 && prev_apv != istrip / 128);
              break;
            case (SiStripPI::MODULE_BASED):
              flush = (prev_det != 0 && prev_det != d);
              break;
            case (SiStripPI::STRIP_BASED):
              flush = (istrip != 0);
              break;
          }

          if (flush) {
            l_mon->Fill(prev_apv, prev_det, enoise.mean());
            enoise.reset();
          }

          enoise.add(std::min<float>(noise, 30.5));
          prev_apv = istrip / 128;
          istrip++;
        }
        prev_det = d;
      }

      auto h_first = f_mon->getHist();
      h_first.SetStats(kFALSE);
      auto h_last = l_mon->getHist();
      h_last.SetStats(kFALSE);

      SiStripPI::makeNicePlotStyle(&h_first);
      SiStripPI::makeNicePlotStyle(&h_last);

      h_first.GetYaxis()->CenterTitle(true);
      h_last.GetYaxis()->CenterTitle(true);

      h_first.GetXaxis()->CenterTitle(true);
      h_last.GetXaxis()->CenterTitle(true);

      h_first.SetLineWidth(2);
      h_last.SetLineWidth(2);

      h_first.SetLineColor(kBlack);
      h_last.SetLineColor(kBlue);

      //=========================
      TCanvas canvas("Partion summary", "partition summary", 1200, 1000);
      canvas.cd();
      canvas.SetBottomMargin(0.11);
      canvas.SetLeftMargin(0.13);
      canvas.SetRightMargin(0.05);
      canvas.Modified();

      float theMax = (h_first.GetMaximum() > h_last.GetMaximum()) ? h_first.GetMaximum() : h_last.GetMaximum();

      h_first.SetMaximum(theMax * 1.30);
      h_last.SetMaximum(theMax * 1.30);

      h_first.Draw();
      h_last.Draw("same");

      TLegend legend = TLegend(0.52, 0.82, 0.95, 0.9);
      legend.SetHeader("SiStrip Noise comparison", "C");  // option "C" allows to center the header
      legend.AddEntry(&h_first, ("IOV: " + std::to_string(std::get<0>(firstiov))).c_str(), "F");
      legend.AddEntry(&h_last, ("IOV: " + std::to_string(std::get<0>(lastiov))).c_str(), "F");
      legend.SetTextSize(0.025);
      legend.Draw("same");

      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());

      return true;
    }

    std::string opType(SiStripPI::OpMode mode) {
      std::string types[3] = {"Strip", "APV", "Module"};
      return types[mode];
    }
  };

  template <SiStripPI::OpMode op_mode_>
  class SiStripNoiseDistributionComparisonSingleTag : public SiStripNoiseDistributionComparisonBase<op_mode_> {
  public:
    SiStripNoiseDistributionComparisonSingleTag() : SiStripNoiseDistributionComparisonBase<op_mode_>() {
      this->setSingleIov(false);
    }
  };

  template <SiStripPI::OpMode op_mode_>
  class SiStripNoiseDistributionComparisonTwoTags : public SiStripNoiseDistributionComparisonBase<op_mode_> {
  public:
    SiStripNoiseDistributionComparisonTwoTags() : SiStripNoiseDistributionComparisonBase<op_mode_>() {
      this->setTwoTags(true);
    }
  };

  typedef SiStripNoiseDistributionComparisonSingleTag<SiStripPI::STRIP_BASED>
      SiStripNoiseValueComparisonPerStripSingleTag;
  typedef SiStripNoiseDistributionComparisonSingleTag<SiStripPI::APV_BASED> SiStripNoiseValueComparisonPerAPVSingleTag;
  typedef SiStripNoiseDistributionComparisonSingleTag<SiStripPI::MODULE_BASED>
      SiStripNoiseValueComparisonPerModuleSingleTag;

  typedef SiStripNoiseDistributionComparisonTwoTags<SiStripPI::STRIP_BASED> SiStripNoiseValueComparisonPerStripTwoTags;
  typedef SiStripNoiseDistributionComparisonTwoTags<SiStripPI::APV_BASED> SiStripNoiseValueComparisonPerAPVTwoTags;
  typedef SiStripNoiseDistributionComparisonTwoTags<SiStripPI::MODULE_BASED> SiStripNoiseValueComparisonPerModuleTwoTags;

  /************************************************
    1d histogram comparison of SiStripNoises of 1 IOV 
  *************************************************/

  // inherit from one of the predefined plot class: PlotImage

  class SiStripNoiseValueComparisonBase : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseValueComparisonBase()
        : cond::payloadInspector::PlotImage<SiStripNoises>("SiStrip Noise values comparison") {}

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      std::vector<std::tuple<cond::Time_t, cond::Hash> > sorted_iovs = iovs;

      // make absolute sure the IOVs are sortd by since
      std::sort(begin(sorted_iovs), end(sorted_iovs), [](auto const& t1, auto const& t2) {
        return std::get<0>(t1) < std::get<0>(t2);
      });

      auto firstiov = sorted_iovs.front();
      auto lastiov = sorted_iovs.back();

      std::shared_ptr<SiStripNoises> f_payload = fetchPayload(std::get<1>(firstiov));
      std::shared_ptr<SiStripNoises> l_payload = fetchPayload(std::get<1>(lastiov));

      auto h_first = std::unique_ptr<TH1F>(
          new TH1F("f_Noise",
                   Form("Strip noise values comparison [%s,%s];Strip Noise [ADC counts];n. strips",
                        std::to_string(std::get<0>(firstiov)).c_str(),
                        std::to_string(std::get<0>(lastiov)).c_str()),
                   100,
                   0.1,
                   10.));
      h_first->SetStats(false);

      auto h_last = std::unique_ptr<TH1F>(
          new TH1F("l_Noise",
                   Form("Strip noise values comparison [%s,%s];Strip Noise [ADC counts];n. strips",
                        std::to_string(std::get<0>(firstiov)).c_str(),
                        std::to_string(std::get<0>(lastiov)).c_str()),
                   100,
                   0.1,
                   10.));
      h_last->SetStats(false);

      std::vector<uint32_t> f_detid;
      f_payload->getDetIds(f_detid);

      // loop on first payload
      for (const auto& d : f_detid) {
        SiStripNoises::Range range = f_payload->getRange(d);
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          float noise = f_payload->getNoise(it, range);
          //to be used to fill the histogram
          h_first->Fill(noise);
        }  // loop over strips
      }

      std::vector<uint32_t> l_detid;
      l_payload->getDetIds(l_detid);

      // loop on first payload
      for (const auto& d : l_detid) {
        SiStripNoises::Range range = l_payload->getRange(d);
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          float noise = l_payload->getNoise(it, range);
          //to be used to fill the histogram
          h_last->Fill(noise);
        }  // loop over strips
      }

      h_first->GetYaxis()->CenterTitle(true);
      h_last->GetYaxis()->CenterTitle(true);

      h_first->GetXaxis()->CenterTitle(true);
      h_last->GetXaxis()->CenterTitle(true);

      h_first->SetLineWidth(2);
      h_last->SetLineWidth(2);

      h_first->SetLineColor(kBlack);
      h_last->SetLineColor(kBlue);

      //=========================
      TCanvas canvas("Partion summary", "partition summary", 1200, 1000);
      canvas.cd();
      canvas.SetBottomMargin(0.11);
      canvas.SetLeftMargin(0.13);
      canvas.SetRightMargin(0.05);
      canvas.Modified();

      float theMax = (h_first->GetMaximum() > h_last->GetMaximum()) ? h_first->GetMaximum() : h_last->GetMaximum();

      h_first->SetMaximum(theMax * 1.30);
      h_last->SetMaximum(theMax * 1.30);

      h_first->Draw();
      h_last->Draw("same");

      TLegend legend = TLegend(0.52, 0.82, 0.95, 0.9);
      legend.SetHeader("SiStrip Noise comparison", "C");  // option "C" allows to center the header
      legend.AddEntry(h_first.get(), ("IOV: " + std::to_string(std::get<0>(firstiov))).c_str(), "F");
      legend.AddEntry(h_last.get(), ("IOV: " + std::to_string(std::get<0>(lastiov))).c_str(), "F");
      legend.SetTextSize(0.025);
      legend.Draw("same");

      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());

      return true;
    }
  };

  class SiStripNoiseValueComparisonSingleTag : public SiStripNoiseValueComparisonBase {
  public:
    SiStripNoiseValueComparisonSingleTag() : SiStripNoiseValueComparisonBase() { setSingleIov(false); }
  };

  class SiStripNoiseValueComparisonTwoTags : public SiStripNoiseValueComparisonBase {
  public:
    SiStripNoiseValueComparisonTwoTags() : SiStripNoiseValueComparisonBase() { setTwoTags(true); }
  };

  /************************************************
    SiStrip Noise Tracker Map 
  *************************************************/

  template <SiStripPI::estimator est>
  class SiStripNoiseTrackerMap : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseTrackerMap()
        : cond::payloadInspector::PlotImage<SiStripNoises>("Tracker Map of SiStripNoise " + estimatorType(est) +
                                                           " per module") {
      setSingleIov(true);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      auto iov = iovs.front();
      std::shared_ptr<SiStripNoises> payload = fetchPayload(std::get<1>(iov));

      std::string titleMap =
          "Tracker Map of Noise " + estimatorType(est) + " per module (payload : " + std::get<1>(iov) + ")";

      std::unique_ptr<TrackerMap> tmap = std::unique_ptr<TrackerMap>(new TrackerMap("SiStripNoises"));
      tmap->setTitle(titleMap);
      tmap->setPalette(1);

      // storage of info
      std::map<unsigned int, float> info_per_detid;

      SiStripNoises::RegistryIterator rit = payload->getRegistryVectorBegin(), erit = payload->getRegistryVectorEnd();
      uint16_t Nstrips;
      std::vector<float> vstripnoise;
      double mean, rms, min, max;
      for (; rit != erit; ++rit) {
        Nstrips =
            (rit->iend - rit->ibegin) * 8 / 9;  //number of strips = number of chars * char size / strip noise size
        vstripnoise.resize(Nstrips);
        payload->allNoises(
            vstripnoise,
            make_pair(payload->getDataVectorBegin() + rit->ibegin, payload->getDataVectorBegin() + rit->iend));
        mean = 0;
        rms = 0;
        min = 10000;
        max = 0;

        DetId detId(rit->detid);

        for (size_t i = 0; i < Nstrips; ++i) {
          mean += vstripnoise[i];
          rms += vstripnoise[i] * vstripnoise[i];
          if (vstripnoise[i] < min)
            min = vstripnoise[i];
          if (vstripnoise[i] > max)
            max = vstripnoise[i];
        }

        mean /= Nstrips;
        if ((rms / Nstrips - mean * mean) > 0.) {
          rms = sqrt(rms / Nstrips - mean * mean);
        } else {
          rms = 0.;
        }

        switch (est) {
          case SiStripPI::min:
            info_per_detid[rit->detid] = min;
            break;
          case SiStripPI::max:
            info_per_detid[rit->detid] = max;
            break;
          case SiStripPI::mean:
            info_per_detid[rit->detid] = mean;
            break;
          case SiStripPI::rms:
            info_per_detid[rit->detid] = rms;
            break;
          default:
            edm::LogWarning("LogicError") << "Unknown estimator: " << est;
            break;
        }
      }

      // loop on the map
      for (const auto& item : info_per_detid) {
        tmap->fill(item.first, item.second);
      }

      auto range = SiStripPI::getTheRange(info_per_detid, 2);

      //=========================

      std::string fileName(m_imageFileName);
      if (est == SiStripPI::rms && (range.first < 0.)) {
        tmap->save(true, 0., range.second, fileName);
      } else {
        tmap->save(true, range.first, range.second, fileName);
      }

      return true;
    }
  };

  typedef SiStripNoiseTrackerMap<SiStripPI::min> SiStripNoiseMin_TrackerMap;
  typedef SiStripNoiseTrackerMap<SiStripPI::max> SiStripNoiseMax_TrackerMap;
  typedef SiStripNoiseTrackerMap<SiStripPI::mean> SiStripNoiseMean_TrackerMap;
  typedef SiStripNoiseTrackerMap<SiStripPI::rms> SiStripNoiseRMS_TrackerMap;

  /************************************************
    SiStrip Noise Tracker Map  (ratio with previous gain per detid)
  *************************************************/

  template <SiStripPI::estimator est, int nsigma>
  class SiStripNoiseRatioWithPreviousIOVTrackerMapBase : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseRatioWithPreviousIOVTrackerMapBase()
        : cond::payloadInspector::PlotImage<SiStripNoises>("Tracker Map of ratio of SiStripNoises " +
                                                           estimatorType(est) + "with previous IOV") {}

    std::map<unsigned int, float> computeEstimator(std::shared_ptr<SiStripNoises> payload) {
      std::map<unsigned int, float> info_per_detid;
      SiStripNoises::RegistryIterator rit = payload->getRegistryVectorBegin(), erit = payload->getRegistryVectorEnd();
      uint16_t Nstrips;
      std::vector<float> vstripnoise;
      double mean, rms, min, max;
      for (; rit != erit; ++rit) {
        Nstrips =
            (rit->iend - rit->ibegin) * 8 / 9;  //number of strips = number of chars * char size / strip noise size
        vstripnoise.resize(Nstrips);
        payload->allNoises(
            vstripnoise,
            make_pair(payload->getDataVectorBegin() + rit->ibegin, payload->getDataVectorBegin() + rit->iend));
        mean = 0;
        rms = 0;
        min = 10000;
        max = 0;

        DetId detId(rit->detid);

        for (size_t i = 0; i < Nstrips; ++i) {
          mean += vstripnoise[i];
          rms += vstripnoise[i] * vstripnoise[i];
          if (vstripnoise[i] < min)
            min = vstripnoise[i];
          if (vstripnoise[i] > max)
            max = vstripnoise[i];
        }

        mean /= Nstrips;
        if ((rms / Nstrips - mean * mean) > 0.) {
          rms = sqrt(rms / Nstrips - mean * mean);
        } else {
          rms = 0.;
        }
        switch (est) {
          case SiStripPI::min:
            info_per_detid[rit->detid] = min;
            break;
          case SiStripPI::max:
            info_per_detid[rit->detid] = max;
            break;
          case SiStripPI::mean:
            info_per_detid[rit->detid] = mean;
            break;
          case SiStripPI::rms:
            info_per_detid[rit->detid] = rms;
            break;
          default:
            edm::LogWarning("LogicError") << "Unknown estimator: " << est;
            break;
        }
      }
      return info_per_detid;
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      std::vector<std::tuple<cond::Time_t, cond::Hash> > sorted_iovs = iovs;

      std::sort(begin(sorted_iovs), end(sorted_iovs), [](auto const& t1, auto const& t2) {
        return std::get<0>(t1) < std::get<0>(t2);
      });

      auto firstiov = sorted_iovs.front();
      auto lastiov = sorted_iovs.back();

      std::shared_ptr<SiStripNoises> last_payload = fetchPayload(std::get<1>(lastiov));
      std::shared_ptr<SiStripNoises> first_payload = fetchPayload(std::get<1>(firstiov));

      std::string titleMap = "SiStripNoise " + estimatorType(est) + " ratio per module average (IOV: ";

      titleMap += std::to_string(std::get<0>(firstiov));
      titleMap += "/ IOV:";
      titleMap += std::to_string(std::get<0>(lastiov));
      titleMap += ")";
      titleMap += +" " + std::to_string(nsigma) + " std. dev. saturation";

      std::unique_ptr<TrackerMap> tmap = std::unique_ptr<TrackerMap>(new TrackerMap("SiStripNoises"));
      tmap->setTitle(titleMap);
      tmap->setPalette(1);

      std::map<unsigned int, float> map_first, map_second;

      map_first = computeEstimator(first_payload);
      map_second = computeEstimator(last_payload);
      std::map<unsigned int, float> cachedRatio;

      for (auto entry : map_first) {
        auto it2 = map_second.find(entry.first);
        if (it2 == map_second.end() || it2->second == 0)
          continue;
        tmap->fill(entry.first, entry.second / it2->second);
        cachedRatio[entry.first] = (entry.second / it2->second);
      }

      auto range = SiStripPI::getTheRange(cachedRatio, nsigma);
      std::string fileName(m_imageFileName);
      if (est == SiStripPI::rms && (range.first < 0.)) {
        tmap->save(true, 0., range.second, fileName);
      } else {
        tmap->save(true, range.first, range.second, fileName);
      }

      return true;
    }
  };

  template <SiStripPI::estimator est, int nsigma>
  class SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag
      : public SiStripNoiseRatioWithPreviousIOVTrackerMapBase<est, nsigma> {
  public:
    SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag()
        : SiStripNoiseRatioWithPreviousIOVTrackerMapBase<est, nsigma>() {
      this->setSingleIov(false);
    }
  };

  template <SiStripPI::estimator est, int nsigma>
  class SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags
      : public SiStripNoiseRatioWithPreviousIOVTrackerMapBase<est, nsigma> {
  public:
    SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags()
        : SiStripNoiseRatioWithPreviousIOVTrackerMapBase<est, nsigma>() {
      this->setTwoTags(true);
    }
  };

  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::min, 1>
      SiStripNoiseMin_RatioWithPreviousIOV1sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::max, 1>
      SiStripNoiseMax_RatioWithPreviousIOV1sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::mean, 1>
      SiStripNoiseMean_RatioWithPreviousIOV1sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::rms, 1>
      SiStripNoiseRms_RatioWithPreviousIOV1sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::min, 2>
      SiStripNoiseMin_RatioWithPreviousIOV2sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::max, 2>
      SiStripNoiseMax_RatioWithPreviousIOV2sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::mean, 2>
      SiStripNoiseMean_RatioWithPreviousIOV2sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::rms, 2>
      SiStripNoiseRms_RatioWithPreviousIOV2sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::min, 3>
      SiStripNoiseMin_RatioWithPreviousIOV3sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::max, 3>
      SiStripNoiseMax_RatioWithPreviousIOV3sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::mean, 3>
      SiStripNoiseMean_RatioWithPreviousIOV3sigmaTrackerMapSingleTag;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapSingleTag<SiStripPI::rms, 3>
      SiStripNoiseRms_RatioWithPreviousIOV3sigmaTrackerMapSingleTag;

  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::min, 1>
      SiStripNoiseMin_RatioWithPreviousIOV1sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::max, 1>
      SiStripNoiseMax_RatioWithPreviousIOV1sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::mean, 1>
      SiStripNoiseMean_RatioWithPreviousIOV1sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::rms, 1>
      SiStripNoiseRms_RatioWithPreviousIOV1sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::min, 2>
      SiStripNoiseMin_RatioWithPreviousIOV2sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::max, 2>
      SiStripNoiseMax_RatioWithPreviousIOV2sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::mean, 2>
      SiStripNoiseMean_RatioWithPreviousIOV2sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::rms, 2>
      SiStripNoiseRms_RatioWithPreviousIOV2sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::min, 3>
      SiStripNoiseMin_RatioWithPreviousIOV3sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::max, 3>
      SiStripNoiseMax_RatioWithPreviousIOV3sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::mean, 3>
      SiStripNoiseMean_RatioWithPreviousIOV3sigmaTrackerMapTwoTags;
  typedef SiStripNoiseRatioWithPreviousIOVTrackerMapTwoTags<SiStripPI::rms, 3>
      SiStripNoiseRms_RatioWithPreviousIOV3sigmaTrackerMapTwoTags;

  /************************************************
  SiStrip Noise Tracker Summaries 
  *************************************************/

  template <SiStripPI::estimator est>
  class SiStripNoiseByRegion : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseByRegion()
        : cond::payloadInspector::PlotImage<SiStripNoises>("SiStrip Noise " + estimatorType(est) + " by Region"),
          m_trackerTopo{StandaloneTrackerTopology::fromTrackerParametersXMLFile(
              edm::FileInPath("Geometry/TrackerCommonData/data/trackerParameters.xml").fullPath())} {
      setSingleIov(true);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      auto iov = iovs.front();
      std::shared_ptr<SiStripNoises> payload = fetchPayload(std::get<1>(iov));

      SiStripDetSummary summaryNoise{&m_trackerTopo};

      SiStripPI::fillNoiseDetSummary(summaryNoise, payload, est);

      std::map<unsigned int, SiStripDetSummary::Values> map = summaryNoise.getCounts();
      //=========================

      TCanvas canvas("Partion summary", "partition summary", 1200, 1000);
      canvas.cd();
      auto h1 = std::unique_ptr<TH1F>(
          new TH1F("byRegion",
                   Form("Average by partition of %s SiStrip Noise per module;;average SiStrip Noise %s [ADC counts]",
                        estimatorType(est).c_str(),
                        estimatorType(est).c_str()),
                   map.size(),
                   0.,
                   map.size()));
      h1->SetStats(false);
      canvas.SetBottomMargin(0.18);
      canvas.SetLeftMargin(0.17);
      canvas.SetRightMargin(0.05);
      canvas.Modified();

      std::vector<int> boundaries;
      unsigned int iBin = 0;

      std::string detector;
      std::string currentDetector;

      for (const auto& element : map) {
        iBin++;
        int count = element.second.count;
        double mean = (element.second.mean) / count;
        double rms = (element.second.rms) / count - mean * mean;

        if (rms <= 0)
          rms = 0;
        else
          rms = sqrt(rms);

        if (currentDetector.empty())
          currentDetector = "TIB";

        switch ((element.first) / 1000) {
          case 1:
            detector = "TIB";
            break;
          case 2:
            detector = "TOB";
            break;
          case 3:
            detector = "TEC";
            break;
          case 4:
            detector = "TID";
            break;
        }

        h1->SetBinContent(iBin, mean);
        h1->GetXaxis()->SetBinLabel(iBin, SiStripPI::regionType(element.first).second);
        h1->GetXaxis()->LabelsOption("v");

        if (detector != currentDetector) {
          boundaries.push_back(iBin);
          currentDetector = detector;
        }
      }

      h1->SetMarkerStyle(20);
      h1->SetMarkerSize(1);
      h1->SetMaximum(h1->GetMaximum() * 1.1);
      h1->Draw("HIST");
      h1->Draw("Psame");

      canvas.Update();

      TLine l[boundaries.size()];
      unsigned int i = 0;

      for (const auto& line : boundaries) {
        l[i] = TLine(h1->GetBinLowEdge(line), canvas.GetUymin(), h1->GetBinLowEdge(line), canvas.GetUymax());
        l[i].SetLineWidth(1);
        l[i].SetLineStyle(9);
        l[i].SetLineColor(2);
        l[i].Draw("same");
        i++;
      }

      TLegend legend = TLegend(0.52, 0.82, 0.95, 0.9);
      legend.SetHeader((std::get<1>(iov)).c_str(), "C");  // option "C" allows to center the header
      legend.AddEntry(h1.get(), ("IOV: " + std::to_string(std::get<0>(iov))).c_str(), "PL");
      legend.SetTextSize(0.025);
      legend.Draw("same");

      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());

      return true;
    }

  private:
    TrackerTopology m_trackerTopo;
  };

  typedef SiStripNoiseByRegion<SiStripPI::mean> SiStripNoiseMeanByRegion;
  typedef SiStripNoiseByRegion<SiStripPI::min> SiStripNoiseMinByRegion;
  typedef SiStripNoiseByRegion<SiStripPI::max> SiStripNoiseMaxByRegion;
  typedef SiStripNoiseByRegion<SiStripPI::rms> SiStripNoiseRMSByRegion;

  /************************************************
  SiStrip Noise Comparator
  *************************************************/

  template <SiStripPI::estimator est>
  class SiStripNoiseComparatorByRegionBase : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseComparatorByRegionBase()
        : cond::payloadInspector::PlotImage<SiStripNoises>("SiStrip Noise " + estimatorType(est) +
                                                           " comparator by Region"),
          m_trackerTopo{StandaloneTrackerTopology::fromTrackerParametersXMLFile(
              edm::FileInPath("Geometry/TrackerCommonData/data/trackerParameters.xml").fullPath())} {}

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      std::vector<std::tuple<cond::Time_t, cond::Hash> > sorted_iovs = iovs;

      // make absolute sure the IOVs are sortd by since
      std::sort(begin(sorted_iovs), end(sorted_iovs), [](auto const& t1, auto const& t2) {
        return std::get<0>(t1) < std::get<0>(t2);
      });

      auto firstiov = sorted_iovs.front();
      auto lastiov = sorted_iovs.back();

      std::shared_ptr<SiStripNoises> f_payload = fetchPayload(std::get<1>(firstiov));
      std::shared_ptr<SiStripNoises> l_payload = fetchPayload(std::get<1>(lastiov));

      SiStripDetSummary f_summaryNoise{&m_trackerTopo};
      SiStripDetSummary l_summaryNoise{&m_trackerTopo};

      SiStripPI::fillNoiseDetSummary(f_summaryNoise, f_payload, est);
      SiStripPI::fillNoiseDetSummary(l_summaryNoise, l_payload, est);

      std::map<unsigned int, SiStripDetSummary::Values> f_map = f_summaryNoise.getCounts();
      std::map<unsigned int, SiStripDetSummary::Values> l_map = l_summaryNoise.getCounts();

      //=========================
      TCanvas canvas("Partion summary", "partition summary", 1200, 1000);
      canvas.cd();

      auto hfirst = std::unique_ptr<TH1F>(
          new TH1F("f_byRegion",
                   Form("Average by partition of %s SiStrip Noise per module;;average SiStrip Noise %s [ADC counts]",
                        estimatorType(est).c_str(),
                        estimatorType(est).c_str()),
                   f_map.size(),
                   0.,
                   f_map.size()));
      hfirst->SetStats(false);

      auto hlast = std::unique_ptr<TH1F>(
          new TH1F("l_byRegion",
                   Form("Average by partition of %s SiStrip Noise per module;;average SiStrip Noise %s [ADC counts]",
                        estimatorType(est).c_str(),
                        estimatorType(est).c_str()),
                   l_map.size(),
                   0.,
                   l_map.size()));
      hlast->SetStats(false);

      canvas.SetBottomMargin(0.18);
      canvas.SetLeftMargin(0.17);
      canvas.SetRightMargin(0.05);
      canvas.Modified();

      std::vector<int> boundaries;
      unsigned int iBin = 0;

      std::string detector;
      std::string currentDetector;

      for (const auto& element : f_map) {
        iBin++;
        int count = element.second.count;
        double mean = (element.second.mean) / count;
        double rms = (element.second.rms) / count - mean * mean;

        if (rms <= 0)
          rms = 0;
        else
          rms = sqrt(rms);

        if (currentDetector.empty())
          currentDetector = "TIB";

        switch ((element.first) / 1000) {
          case 1:
            detector = "TIB";
            break;
          case 2:
            detector = "TOB";
            break;
          case 3:
            detector = "TEC";
            break;
          case 4:
            detector = "TID";
            break;
        }

        hfirst->SetBinContent(iBin, mean);
        //hfirst->SetBinError(iBin,rms);
        hfirst->GetXaxis()->SetBinLabel(iBin, SiStripPI::regionType(element.first).second);
        hfirst->GetXaxis()->LabelsOption("v");

        if (detector != currentDetector) {
          boundaries.push_back(iBin);
          currentDetector = detector;
        }
      }

      // second payload
      // reset the counter
      iBin = 0;

      for (const auto& element : l_map) {
        iBin++;
        int count = element.second.count;
        double mean = (element.second.mean) / count;
        double rms = (element.second.rms) / count - mean * mean;

        if (rms <= 0)
          rms = 0;
        else
          rms = sqrt(rms);

        hlast->SetBinContent(iBin, mean);
        //hlast->SetBinError(iBin,rms);
        hlast->GetXaxis()->SetBinLabel(iBin, SiStripPI::regionType(element.first).second);
        hlast->GetXaxis()->LabelsOption("v");
      }

      float theMax = (hfirst->GetMaximum() > hlast->GetMaximum()) ? hfirst->GetMaximum() : hlast->GetMaximum();
      float theMin = (hfirst->GetMinimum() < hlast->GetMinimum()) ? hfirst->GetMinimum() : hlast->GetMinimum();

      hfirst->SetMarkerStyle(20);
      hfirst->SetMarkerSize(1);
      hfirst->GetYaxis()->SetTitleOffset(1.3);
      hfirst->GetYaxis()->SetRangeUser(theMin * 0.9, theMax * 1.1);
      hfirst->Draw("HIST");
      hfirst->Draw("Psame");

      hlast->SetMarkerStyle(21);
      hlast->SetMarkerSize(1);
      hlast->SetMarkerColor(kBlue);
      hlast->SetLineColor(kBlue);
      hlast->GetYaxis()->SetTitleOffset(1.3);
      hlast->GetYaxis()->SetRangeUser(theMin * 0.9, theMax * 1.1);
      hlast->Draw("HISTsame");
      hlast->Draw("Psame");

      canvas.Update();

      TLine l[boundaries.size()];
      unsigned int i = 0;

      for (const auto& line : boundaries) {
        l[i] = TLine(hfirst->GetBinLowEdge(line), canvas.GetUymin(), hfirst->GetBinLowEdge(line), canvas.GetUymax());
        l[i].SetLineWidth(1);
        l[i].SetLineStyle(9);
        l[i].SetLineColor(2);
        l[i].Draw("same");
        i++;
      }

      TLegend legend = TLegend(0.52, 0.82, 0.95, 0.9);
      legend.SetHeader(("SiStrip Noise " + estimatorType(est) + " by region").c_str(),
                       "C");  // option "C" allows to center the header
      legend.AddEntry(hfirst.get(), ("IOV: " + std::to_string(std::get<0>(firstiov))).c_str(), "PL");
      legend.AddEntry(hlast.get(), ("IOV: " + std::to_string(std::get<0>(lastiov))).c_str(), "PL");
      legend.SetTextSize(0.025);
      legend.Draw("same");

      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());

      return true;
    }

  private:
    TrackerTopology m_trackerTopo;
  };

  template <SiStripPI::estimator est>
  class SiStripNoiseComparatorByRegionSingleTag : public SiStripNoiseComparatorByRegionBase<est> {
  public:
    SiStripNoiseComparatorByRegionSingleTag() : SiStripNoiseComparatorByRegionBase<est>() { this->setSingleIov(false); }
  };

  template <SiStripPI::estimator est>
  class SiStripNoiseComparatorByRegionTwoTags : public SiStripNoiseComparatorByRegionBase<est> {
  public:
    SiStripNoiseComparatorByRegionTwoTags() : SiStripNoiseComparatorByRegionBase<est>() { this->setTwoTags(true); }
  };

  typedef SiStripNoiseComparatorByRegionSingleTag<SiStripPI::mean> SiStripNoiseComparatorMeanByRegionSingleTag;
  typedef SiStripNoiseComparatorByRegionSingleTag<SiStripPI::min> SiStripNoiseComparatorMinByRegionSingleTag;
  typedef SiStripNoiseComparatorByRegionSingleTag<SiStripPI::max> SiStripNoiseComparatorMaxByRegionSingleTag;
  typedef SiStripNoiseComparatorByRegionSingleTag<SiStripPI::rms> SiStripNoiseComparatorRMSByRegionSingleTag;

  typedef SiStripNoiseComparatorByRegionTwoTags<SiStripPI::mean> SiStripNoiseComparatorMeanByRegionTwoTags;
  typedef SiStripNoiseComparatorByRegionTwoTags<SiStripPI::min> SiStripNoiseComparatorMinByRegionTwoTags;
  typedef SiStripNoiseComparatorByRegionTwoTags<SiStripPI::max> SiStripNoiseComparatorMaxByRegionTwoTags;
  typedef SiStripNoiseComparatorByRegionTwoTags<SiStripPI::rms> SiStripNoiseComparatorRMSByRegionTwoTags;

  /************************************************
    Noise linearity
  *************************************************/
  class SiStripNoiseLinearity : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    SiStripNoiseLinearity()
        : cond::payloadInspector::PlotImage<SiStripNoises>("Linearity of Strip Noise as a fuction of strip length") {
      setSingleIov(true);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      auto iov = iovs.front();
      std::shared_ptr<SiStripNoises> payload = fetchPayload(std::get<1>(iov));

      edm::FileInPath fp_ = edm::FileInPath("CalibTracker/SiStripCommon/data/SiStripDetInfo.dat");
      SiStripDetInfoFileReader* reader = new SiStripDetInfoFileReader(fp_.fullPath());

      std::vector<uint32_t> detid;
      payload->getDetIds(detid);

      std::map<float, std::tuple<int, float, float> > noisePerStripLength;

      for (const auto& d : detid) {
        SiStripNoises::Range range = payload->getRange(d);
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          auto noise = payload->getNoise(it, range);
          //to be used to fill the histogram
          float stripL = reader->getNumberOfApvsAndStripLength(d).second;
          std::get<0>(noisePerStripLength[stripL]) += 1;
          std::get<1>(noisePerStripLength[stripL]) += noise;
          std::get<2>(noisePerStripLength[stripL]) += (noise * noise);
        }  // loop over strips
      }    // loop over detIds

      TCanvas canvas("Noise linearity", "noise linearity", 1200, 1000);
      canvas.cd();

      std::vector<float> x;
      x.reserve(noisePerStripLength.size());
      std::vector<float> y;
      y.reserve(noisePerStripLength.size());
      std::vector<float> ex;
      ex.reserve(noisePerStripLength.size());
      std::vector<float> ey;
      ey.reserve(noisePerStripLength.size());

      for (const auto& element : noisePerStripLength) {
        x.push_back(element.first);
        ex.push_back(0.);
        float sum = std::get<1>(element.second);
        float sum2 = std::get<2>(element.second);
        float nstrips = std::get<0>(element.second);
        float mean = sum / nstrips;
        float rms = (sum2 / nstrips - mean * mean) > 0. ? sqrt(sum2 / nstrips - mean * mean) : 0.;
        y.push_back(mean);
        ey.push_back(rms);
        //std::cout<<" strip lenght: " << element.first << " avg noise=" << mean <<" +/-" << rms << std::endl;
      }

      auto graph =
          std::unique_ptr<TGraphErrors>(new TGraphErrors(noisePerStripLength.size(), &x[0], &y[0], &ex[0], &ey[0]));
      graph->SetTitle("SiStrip Noise Linearity");
      graph->GetXaxis()->SetTitle("Strip length [cm]");
      graph->GetYaxis()->SetTitle("Average Strip Noise [ADC counts]");
      graph->SetMarkerColor(kBlue);
      graph->SetMarkerStyle(20);
      graph->SetMarkerSize(1.5);
      canvas.SetBottomMargin(0.13);
      canvas.SetLeftMargin(0.17);
      canvas.SetTopMargin(0.08);
      canvas.SetRightMargin(0.05);
      canvas.Modified();
      canvas.cd();

      graph->GetXaxis()->CenterTitle(true);
      graph->GetYaxis()->CenterTitle(true);
      graph->GetXaxis()->SetTitleFont(42);
      graph->GetYaxis()->SetTitleFont(42);
      graph->GetXaxis()->SetTitleSize(0.05);
      graph->GetYaxis()->SetTitleSize(0.05);
      graph->GetXaxis()->SetTitleOffset(1.1);
      graph->GetYaxis()->SetTitleOffset(1.3);
      graph->GetXaxis()->SetLabelFont(42);
      graph->GetYaxis()->SetLabelFont(42);
      graph->GetYaxis()->SetLabelSize(.05);
      graph->GetXaxis()->SetLabelSize(.05);

      graph->Draw("AP");
      graph->Fit("pol1");
      //Access the fit resuts
      TF1* f1 = graph->GetFunction("pol1");
      f1->SetLineWidth(2);
      f1->SetLineColor(kBlue);
      f1->Draw("same");

      auto fits = std::unique_ptr<TPaveText>(new TPaveText(0.2, 0.72, 0.6, 0.9, "NDC"));
      char buffer[255];
      sprintf(buffer, "fit function: p_{0} + p_{1} * l_{strip}");
      fits->AddText(buffer);
      sprintf(buffer, "p_{0} : %5.2f [ADC counts]", f1->GetParameter(0));
      fits->AddText(buffer);
      sprintf(buffer, "p_{1} : %5.2f [ADC counts/cm]", f1->GetParameter(1));
      fits->AddText(buffer);
      sprintf(buffer, "#chi^{2}/ndf = %5.2f / %i ", f1->GetChisquare(), f1->GetNDF());
      fits->AddText(buffer);
      fits->SetTextFont(42);
      fits->SetTextColor(kBlue);
      fits->SetFillColor(0);
      fits->SetTextSize(0.03);
      fits->SetBorderSize(1);
      fits->SetLineColor(kBlue);
      fits->SetMargin(0.05);
      fits->SetTextAlign(12);
      fits->Draw();

      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());

      delete f1;
      delete reader;
      return true;
    }
  };

  /************************************************
   template Noise history per subdetector
  *************************************************/

  template <StripSubdetector::SubDetector sub>
  class NoiseHistory : public cond::payloadInspector::HistoryPlot<SiStripNoises, std::pair<double, double> > {
  public:
    NoiseHistory()
        : cond::payloadInspector::HistoryPlot<SiStripNoises, std::pair<double, double> >(
              "Average " + SiStripPI::getStringFromSubdet(sub) + " noise vs run number",
              "average " + SiStripPI::getStringFromSubdet(sub) + " Noise") {}

    std::pair<double, double> getFromPayload(SiStripNoises& payload) override {
      std::vector<uint32_t> detid;
      payload.getDetIds(detid);

      int nStrips = 0;
      float sum = 0., sum2 = 0.;

      for (const auto& d : detid) {
        int subid = DetId(d).subdetId();
        if (subid != sub)
          continue;
        SiStripNoises::Range range = payload.getRange(d);
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          nStrips++;
          auto noise = payload.getNoise(it, range);
          sum += noise;
          sum2 += (noise * noise);
        }  // loop on strips
      }    // loop on detIds

      float mean = sum / nStrips;
      float rms = (sum2 / nStrips - mean * mean) > 0. ? sqrt(sum2 / nStrips - mean * mean) : 0.;

      return std::make_pair(mean, rms);

    }  // close getFromPayload
  };

  typedef NoiseHistory<StripSubdetector::TIB> TIBNoiseHistory;
  typedef NoiseHistory<StripSubdetector::TOB> TOBNoiseHistory;
  typedef NoiseHistory<StripSubdetector::TID> TIDNoiseHistory;
  typedef NoiseHistory<StripSubdetector::TEC> TECNoiseHistory;

  /************************************************
   template Noise run history  per subdetector
  *************************************************/

  template <StripSubdetector::SubDetector sub>
  class NoiseRunHistory : public cond::payloadInspector::RunHistoryPlot<SiStripNoises, std::pair<double, double> > {
  public:
    NoiseRunHistory()
        : cond::payloadInspector::RunHistoryPlot<SiStripNoises, std::pair<double, double> >(
              "Average " + SiStripPI::getStringFromSubdet(sub) + " noise vs run number",
              "average " + SiStripPI::getStringFromSubdet(sub) + " Noise") {}

    std::pair<double, double> getFromPayload(SiStripNoises& payload) override {
      std::vector<uint32_t> detid;
      payload.getDetIds(detid);

      int nStrips = 0;
      float sum = 0., sum2 = 0.;

      for (const auto& d : detid) {
        int subid = DetId(d).subdetId();
        if (subid != sub)
          continue;
        SiStripNoises::Range range = payload.getRange(d);
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          nStrips++;
          auto noise = payload.getNoise(it, range);
          sum += noise;
          sum2 += (noise * noise);
        }  // loop on strips
      }    // loop on detIds

      float mean = sum / nStrips;
      float rms = (sum2 / nStrips - mean * mean) > 0. ? sqrt(sum2 / nStrips - mean * mean) : 0.;

      return std::make_pair(mean, rms);

    }  // close getFromPayload
  };

  typedef NoiseRunHistory<StripSubdetector::TIB> TIBNoiseRunHistory;
  typedef NoiseRunHistory<StripSubdetector::TOB> TOBNoiseRunHistory;
  typedef NoiseRunHistory<StripSubdetector::TID> TIDNoiseRunHistory;
  typedef NoiseRunHistory<StripSubdetector::TEC> TECNoiseRunHistory;

  /************************************************
   template Noise Time history per subdetector
  *************************************************/

  template <StripSubdetector::SubDetector sub>
  class NoiseTimeHistory : public cond::payloadInspector::TimeHistoryPlot<SiStripNoises, std::pair<double, double> > {
  public:
    NoiseTimeHistory()
        : cond::payloadInspector::TimeHistoryPlot<SiStripNoises, std::pair<double, double> >(
              "Average " + SiStripPI::getStringFromSubdet(sub) + " noise vs run number",
              "average " + SiStripPI::getStringFromSubdet(sub) + " Noise") {}

    std::pair<double, double> getFromPayload(SiStripNoises& payload) override {
      std::vector<uint32_t> detid;
      payload.getDetIds(detid);

      int nStrips = 0;
      float sum = 0., sum2 = 0.;

      for (const auto& d : detid) {
        int subid = DetId(d).subdetId();
        if (subid != sub)
          continue;
        SiStripNoises::Range range = payload.getRange(d);
        for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
          nStrips++;
          auto noise = payload.getNoise(it, range);
          sum += noise;
          sum2 += (noise * noise);
        }  // loop on strips
      }    // loop on detIds

      float mean = sum / nStrips;
      float rms = (sum2 / nStrips - mean * mean) > 0. ? sqrt(sum2 / nStrips - mean * mean) : 0.;

      return std::make_pair(mean, rms);

    }  // close getFromPayload
  };

  typedef NoiseTimeHistory<StripSubdetector::TIB> TIBNoiseTimeHistory;
  typedef NoiseTimeHistory<StripSubdetector::TOB> TOBNoiseTimeHistory;
  typedef NoiseTimeHistory<StripSubdetector::TID> TIDNoiseTimeHistory;
  typedef NoiseTimeHistory<StripSubdetector::TEC> TECNoiseTimeHistory;

  /************************************************
   template Noise run history  per layer
  *************************************************/
  template <StripSubdetector::SubDetector sub>
  class NoiseLayerRunHistory : public cond::payloadInspector::PlotImage<SiStripNoises> {
  public:
    NoiseLayerRunHistory()
        : cond::payloadInspector::PlotImage<SiStripNoises>("SiStrip Noise values comparison"),
          m_trackerTopo{StandaloneTrackerTopology::fromTrackerParametersXMLFile(
              edm::FileInPath("Geometry/TrackerCommonData/data/trackerParameters.xml").fullPath())} {
      setSingleIov(false);
    }

    bool fill(const std::vector<std::tuple<cond::Time_t, cond::Hash> >& iovs) override {
      std::vector<std::tuple<cond::Time_t, cond::Hash> > sorted_iovs = iovs;

      // make absolute sure the IOVs are sortd by since
      std::sort(begin(sorted_iovs), end(sorted_iovs), [](auto const& t1, auto const& t2) {
        return std::get<0>(t1) < std::get<0>(t2);
      });

      std::unordered_map<int, std::vector<float> > noises_avg;
      std::unordered_map<int, std::vector<float> > noises_err;
      std::vector<float> runs;
      std::vector<float> runs_err;

      for (auto const& iov : sorted_iovs) {
        std::unordered_map<int, std::vector<float> > noises;  //map with noises per layer

        std::shared_ptr<SiStripNoises> payload = fetchPayload(std::get<1>(iov));
        unsigned int run = std::get<0>(iov);
        runs.push_back(run);
        runs_err.push_back(0);

        if (payload.get()) {
          std::vector<uint32_t> detid;
          payload->getDetIds(detid);

          for (const auto& d : detid) {
            int subid = DetId(d).subdetId();
            int layer = -1;
            if (subid != sub)
              continue;
            if (subid == StripSubdetector::TIB) {
              layer = m_trackerTopo.tibLayer(d);
            } else if (subid == StripSubdetector::TOB) {
              layer = m_trackerTopo.tobLayer(d);
            } else if (subid == StripSubdetector::TID) {
              layer = m_trackerTopo.tidWheel(d);
            } else if (subid == StripSubdetector::TEC) {
              layer = m_trackerTopo.tecWheel(d);
            }

            SiStripNoises::Range range = payload->getRange(d);
            for (int it = 0; it < (range.second - range.first) * 8 / 9; ++it) {
              auto noise = payload->getNoise(it, range);
              if (noises.find(layer) == noises.end())
                noises.emplace(layer, std::vector<float>{});
              noises[layer].push_back(noise);
            }  // loop on strips
          }    // loop on detIds

          for (auto& entry : noises) {
            double sum = std::accumulate(entry.second.begin(), entry.second.end(), 0.0);
            double mean = sum / entry.second.size();

            //double sq_sum = std::inner_product(entry.second.begin(), entry.second.end(), entry.second.begin(), 0.0);
            //double stdev = std::sqrt(sq_sum / entry.second.size() - mean * mean);

            if (noises_avg.find(entry.first) == noises_avg.end())
              noises_avg.emplace(entry.first, std::vector<float>{});
            noises_avg[entry.first].push_back(mean);

            if (noises_err.find(entry.first) == noises_err.end())
              noises_err.emplace(entry.first, std::vector<float>{});
            noises_err[entry.first].push_back(0);
          }  //get
        }    //run on iov
      }
      TCanvas canvas("Partion summary", "partition summary", 2000, 1000);
      canvas.cd();
      canvas.SetBottomMargin(0.11);
      canvas.SetLeftMargin(0.13);
      canvas.SetRightMargin(0.05);
      canvas.Modified();

      TLegend legend = TLegend(0.73, 0.13, 0.89, 0.43);
      //legend.SetHeader("Layers","C"); // option "C" allows to center the header
      legend.SetTextSize(0.03);

      std::unique_ptr<TGraphErrors> graph[noises_avg.size()];

      int colors[18] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 20, 30, 40, 42, 46, 48, 32, 36, 38};

      int el = 0;

      for (auto& entry : noises_avg) {
        graph[el] = std::unique_ptr<TGraphErrors>(
            new TGraphErrors(runs.size(), &runs[0], &(entry.second[0]), &runs_err[0], &(noises_err[entry.first][0])));
        char title[100];
        char name[100];
        snprintf(name, sizeof(name), "gr%d", entry.first);
        graph[el]->SetName(name);

        if (sub == StripSubdetector::TIB) {
          snprintf(title, sizeof(title), "SiStrip avg noise per layer -- TIB");
          graph[el]->GetYaxis()->SetTitle("Average Noise per Layer [ADC counts]");
        } else if (sub == StripSubdetector::TOB) {
          snprintf(title, sizeof(title), "SiStrip avg noise per layer -- TOB");
          graph[el]->GetYaxis()->SetTitle("Average Noise per Layer [ADC counts]");
        } else if (sub == StripSubdetector::TID) {
          snprintf(title, sizeof(title), "SiStrip avg noise per disk -- TID");
          graph[el]->GetYaxis()->SetTitle("Average Noise per Disk [ADC counts]");
        } else if (sub == StripSubdetector::TEC) {
          snprintf(title, sizeof(title), "SiStrip avg noise per disk -- TEC");
          graph[el]->GetYaxis()->SetTitle("Average Noise per Disk [ADC counts]");
        }

        graph[el]->SetTitle(title);
        graph[el]->GetXaxis()->SetTitle("run");
        graph[el]->SetMarkerColor(colors[el]);
        graph[el]->SetMarkerStyle(20);
        graph[el]->SetMarkerSize(1.5);
        graph[el]->GetXaxis()->CenterTitle(true);
        graph[el]->GetYaxis()->CenterTitle(true);
        graph[el]->GetXaxis()->SetTitleFont(42);
        graph[el]->GetYaxis()->SetTitleFont(42);
        graph[el]->GetXaxis()->SetTitleSize(0.05);
        graph[el]->GetYaxis()->SetTitleSize(0.05);
        graph[el]->GetXaxis()->SetTitleOffset(1.1);
        graph[el]->GetYaxis()->SetTitleOffset(1.3);
        graph[el]->GetXaxis()->SetLabelFont(42);
        graph[el]->GetYaxis()->SetLabelFont(42);
        graph[el]->GetYaxis()->SetLabelSize(.05);
        graph[el]->GetXaxis()->SetLabelSize(.05);
        graph[el]->SetMinimum(3);
        graph[el]->SetMaximum(7.5);

        if (el == 0)
          graph[el]->Draw("AP");
        else
          graph[el]->Draw("P");

        if (sub == StripSubdetector::TIB) {
          legend.AddEntry(name, ("layer " + std::to_string(entry.first)).c_str(), "lep");
        } else if (sub == StripSubdetector::TOB) {
          legend.AddEntry(name, ("layer " + std::to_string(entry.first)).c_str(), "lep");
        } else if (sub == StripSubdetector::TID) {
          legend.AddEntry(name, ("disk " + std::to_string(entry.first)).c_str(), "lep");
        } else if (sub == StripSubdetector::TEC) {
          legend.AddEntry(name, ("disk " + std::to_string(entry.first)).c_str(), "lep");
        }

        if (el == 0)
          legend.Draw();
        else
          legend.Draw("same");
        el++;
      }
      //canvas.BuildLegend();
      std::string fileName(m_imageFileName);
      canvas.SaveAs(fileName.c_str());
      return true;
    }

  private:
    TrackerTopology m_trackerTopo;
  };

  typedef NoiseLayerRunHistory<StripSubdetector::TIB> TIBNoiseLayerRunHistory;
  typedef NoiseLayerRunHistory<StripSubdetector::TOB> TOBNoiseLayerRunHistory;
  typedef NoiseLayerRunHistory<StripSubdetector::TID> TIDNoiseLayerRunHistory;
  typedef NoiseLayerRunHistory<StripSubdetector::TEC> TECNoiseLayerRunHistory;

}  // namespace

PAYLOAD_INSPECTOR_MODULE(SiStripNoises) {
  PAYLOAD_INSPECTOR_CLASS(SiStripNoisesTest);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValue);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValuePerStrip);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValuePerAPV);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValuePerModule);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonPerStripSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonPerAPVSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonPerModuleSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonPerStripTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonPerAPVTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseValueComparisonPerModuleTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_TrackerMap);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_TrackerMap);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_TrackerMap);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRMS_TrackerMap);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMeanByRegion);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMinByRegion);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMaxByRegion);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRMSByRegion);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorMeanByRegionSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorMinByRegionSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorMaxByRegionSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorRMSByRegionSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorMeanByRegionTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorMinByRegionTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorMaxByRegionTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseComparatorRMSByRegionTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_RatioWithPreviousIOV1sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_RatioWithPreviousIOV1sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_RatioWithPreviousIOV1sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRms_RatioWithPreviousIOV1sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_RatioWithPreviousIOV2sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_RatioWithPreviousIOV2sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_RatioWithPreviousIOV2sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRms_RatioWithPreviousIOV2sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_RatioWithPreviousIOV3sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_RatioWithPreviousIOV3sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_RatioWithPreviousIOV3sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRms_RatioWithPreviousIOV3sigmaTrackerMapSingleTag);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_RatioWithPreviousIOV1sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_RatioWithPreviousIOV1sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_RatioWithPreviousIOV1sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRms_RatioWithPreviousIOV1sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_RatioWithPreviousIOV2sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_RatioWithPreviousIOV2sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_RatioWithPreviousIOV2sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRms_RatioWithPreviousIOV2sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMin_RatioWithPreviousIOV3sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMax_RatioWithPreviousIOV3sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseMean_RatioWithPreviousIOV3sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseRms_RatioWithPreviousIOV3sigmaTrackerMapTwoTags);
  PAYLOAD_INSPECTOR_CLASS(SiStripNoiseLinearity);
  PAYLOAD_INSPECTOR_CLASS(TIBNoiseHistory);
  PAYLOAD_INSPECTOR_CLASS(TOBNoiseHistory);
  PAYLOAD_INSPECTOR_CLASS(TIDNoiseHistory);
  PAYLOAD_INSPECTOR_CLASS(TECNoiseHistory);
  PAYLOAD_INSPECTOR_CLASS(TIBNoiseRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TOBNoiseRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TIDNoiseRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TECNoiseRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TIBNoiseLayerRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TOBNoiseLayerRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TIDNoiseLayerRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TECNoiseLayerRunHistory);
  PAYLOAD_INSPECTOR_CLASS(TIBNoiseTimeHistory);
  PAYLOAD_INSPECTOR_CLASS(TOBNoiseTimeHistory);
  PAYLOAD_INSPECTOR_CLASS(TIDNoiseTimeHistory);
  PAYLOAD_INSPECTOR_CLASS(TECNoiseTimeHistory);
}
