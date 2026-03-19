#ifndef SUEPLIMITS_SUEPLIMITS_BIN_SYSTEMATICNAMING_H_
#define SUEPLIMITS_SUEPLIMITS_BIN_SYSTEMATICNAMING_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CombineHarvester/CombineTools/interface/CombineHarvester.h"
#include "CombineHarvester/CombineTools/interface/Systematic.h"

namespace suep {

inline std::string remap_systematic_name(const std::string& old_name) {
  static const std::unordered_map<std::string, std::string> kExactNameMap = {
      // Systematircs split by Run
      {"PUReweight_13TeV", "CMS_pileup_13TeV"},
      {"PUReweight_13p6TeV", "CMS_pileup_13p6TeV"},
      {"LHEPdf_13TeV", "pdf_13TeV"},
      {"LHEPdf_13p6TeV", "pdf_13p6TeV"},
      {"LHEScaleMuF_13TeV", "fac_scale_13TeV"},
      {"LHEScaleMuF_13p6TeV", "fac_scale_13p6TeV"},
      {"LHEScaleMuR_13TeV", "ren_scale_13TeV"},
      {"LHEScaleMuR_13p6TeV", "ren_scale_13p6TeV"},
      {"ISR_13TeV", "ps_isr_13TeV"},
      {"ISR_13p6TeV", "ps_isr_13p6TeV"},
      {"FSR_13TeV", "ps_fsr_13TeV"},
      {"FSR_13p6TeV", "ps_fsr_13p6TeV"},
      // Systematics split by year
      {"TrigSF_13TeV_2016", "CMS_eff_m_trigger_2016"},
      {"TrigSF_13TeV_2017", "CMS_eff_m_trigger_2017"},
      {"TrigSF_13TeV_2018", "CMS_eff_m_trigger_2018"},
      {"TrigSF_13p6TeV_2022", "CMS_eff_m_trigger_2022"},
      {"TrigSF_13p6TeV_2022EE", "CMS_eff_m_trigger_2022EE"},
      {"TrigSF_13p6TeV_2023", "CMS_eff_m_trigger_2023"},
      {"TrigSF_13p6TeV_2023BPix", "CMS_eff_m_trigger_2023BPix"},
      {"L1PreFire_13TeV_2016", "CMS_l1_muon_prefiring_2016"},
      {"L1PreFire_13TeV_2017", "CMS_l1_muon_prefiring_2017"},
      {"L1PreFire_13TeV_2018", "CMS_l1_muon_prefiring_2018"},
      {"L1PreFire_13p6TeV_2022", "CMS_l1_muon_prefiring_2022"},
      {"L1PreFire_13p6TeV_2022EE", "CMS_l1_muon_prefiring_2022EE"},
      {"L1PreFire_13p6TeV_2023", "CMS_l1_muon_prefiring_2023"},
      {"L1PreFire_13p6TeV_2023BPix", "CMS_l1_muon_prefiring_2023BPix"},
      {"MuonSF_13TeV_2016", "CMS_eff_m_2016"},
      {"MuonSF_13TeV_2017", "CMS_eff_m_2017"},
      {"MuonSF_13TeV_2018", "CMS_eff_m_2018"},
      {"MuonSF_13p6TeV_2022", "CMS_eff_m_2022"},
      {"MuonSF_13p6TeV_2022EE", "CMS_eff_m_2022EE"},
      {"MuonSF_13p6TeV_2023", "CMS_eff_m_2023"},
      {"MuonSF_13p6TeV_2023BPix", "CMS_eff_m_2023BPix"},
      {"TrkEff_13TeV_2016", "CMS_EXO25011_TrkEff_2016"},
      {"TrkEff_13TeV_2017", "CMS_EXO25011_TrkEff_2017"},
      {"TrkEff_13TeV_2018", "CMS_EXO25011_TrkEff_2018"},
      {"TrkEff_13p6TeV_2022", "CMS_EXO25011_TrkEff_2022"},
      {"TrkEff_13p6TeV_2022EE", "CMS_EXO25011_TrkEff_2022EE"},
      {"TrkEff_13p6TeV_2023", "CMS_EXO25011_TrkEff_2023"},
      {"TrkEff_13p6TeV_2023BPix", "CMS_EXO25011_TrkEff_2023BPix"},
      {"MCStatSUEPSRHighTBin0_13TeV_2016", "CMS_EXO25011_srmcstat_SUEP_2016"},
      {"MCStatSUEPSRHighTBin0_13TeV_2017", "CMS_EXO25011_srmcstat_SUEP_2017"},
      {"MCStatSUEPSRHighTBin0_13TeV_2018", "CMS_EXO25011_srmcstat_SUEP_2018"},
      {"MCStatSUEPSRHighTBin0_13p6TeV_2022", "CMS_EXO25011_srmcstat_SUEP_2022"},
      {"MCStatSUEPSRHighTBin0_13p6TeV_2022EE", "CMS_EXO25011_srmcstat_SUEP_2022EE"},
      {"MCStatSUEPSRHighTBin0_13p6TeV_2023", "CMS_EXO25011_srmcstat_SUEP_2023"},
      {"MCStatSUEPSRHighTBin0_13p6TeV_2023BPix", "CMS_EXO25011_srmcstat_SUEP_2023BPix"},
      {"MCStatQCDSRHighTBin0_13TeV_2016", "CMS_EXO25011_srmcstat_QCD_2016"},
      {"MCStatQCDSRHighTBin0_13TeV_2017", "CMS_EXO25011_srmcstat_QCD_2017"},
      {"MCStatQCDSRHighTBin0_13TeV_2018", "CMS_EXO25011_srmcstat_QCD_2018"},
      {"MCStatQCDSRHighTBin0_13p6TeV_2022", "CMS_EXO25011_srmcstat_QCD_2022"},
      {"MCStatQCDSRHighTBin0_13p6TeV_2022EE", "CMS_EXO25011_srmcstat_QCD_2022EE"},
      {"MCStatQCDSRHighTBin0_13p6TeV_2023", "CMS_EXO25011_srmcstat_QCD_2023"},
      {"MCStatQCDSRHighTBin0_13p6TeV_2023BPix", "CMS_EXO25011_srmcstat_QCD_2023BPix"},
      {"MCStatDYSRHighTBin0_13TeV_2016", "CMS_EXO25011_srmcstat_DY_2016"},
      {"MCStatDYSRHighTBin0_13TeV_2017", "CMS_EXO25011_srmcstat_DY_2017"},
      {"MCStatDYSRHighTBin0_13TeV_2018", "CMS_EXO25011_srmcstat_DY_2018"},
      {"MCStatDYSRHighTBin0_13p6TeV_2022", "CMS_EXO25011_srmcstat_DY_2022"},
      {"MCStatDYSRHighTBin0_13p6TeV_2022EE", "CMS_EXO25011_srmcstat_DY_2022EE"},
      {"MCStatDYSRHighTBin0_13p6TeV_2023", "CMS_EXO25011_srmcstat_DY_2023"},
      {"MCStatDYSRHighTBin0_13p6TeV_2023BPix", "CMS_EXO25011_srmcstat_DY_2023BPix"},
  };

  auto exact_match = kExactNameMap.find(old_name);
  if (exact_match != kExactNameMap.end()) return exact_match->second;

  return old_name;
}

inline void apply_systematic_name_map(ch::CombineHarvester& cb) {
  cb.ForEachSyst([&](ch::Systematic* sys) {
    const std::string old_name = sys->name();
    const std::string new_name = remap_systematic_name(old_name);
    if (new_name == old_name) return;

    sys->set_name(new_name);
    cb.RenameParameter(old_name, new_name);
  });
}

}  // namespace suep

#endif  // SUEPLIMITS_SUEPLIMITS_BIN_SYSTEMATICNAMING_H_
