#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <tuple>
#include "SystematicNaming.h"
#include "CombineHarvester/CombineTools/interface/CombineHarvester.h"
#include "CombineHarvester/CombineTools/interface/Observation.h"
#include "CombineHarvester/CombineTools/interface/Process.h"
#include "CombineHarvester/CombineTools/interface/Utilities.h"
#include "CombineHarvester/CombineTools/interface/Systematics.h"
#include "CombineHarvester/CombineTools/interface/BinByBin.h"

using namespace std;

string get_filename(string filename) {
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != string::npos) {
        filename = filename.substr(last_slash + 1);
    }
    size_t last_dot = filename.find_last_of(".");
    if (last_dot != string::npos) {
        filename = filename.substr(0, last_dot);
    }
    return filename;
}

string get_com_energy(const string& year) {
    if (year.substr(0, 3) == "201") return "13TeV";
    if (year.substr(0, 3) == "202") return "13p6TeV";
    throw runtime_error("Invalid year format: " + year);
}

bool check_histogram_exists(TFile* file, const string& hist_name) {
    if (!file || file->IsZombie()) return false;
    TH1* hist = dynamic_cast<TH1*>(file->Get(hist_name.c_str()));
    return (hist != nullptr);
}

TH1* get_histogram(TFile* file, const string& hist_name, bool warn_if_missing = true) {
    if (!file || file->IsZombie()) {
        if (warn_if_missing) {
            cerr << "Warning: invalid ROOT file while looking for histogram " << hist_name << endl;
        }
        return nullptr;
    }

    TH1* hist = dynamic_cast<TH1*>(file->Get(hist_name.c_str()));
    if (!hist && warn_if_missing) {
        cerr << "Warning: Histogram " << hist_name << " does not exist in file: "
             << file->GetName() << endl;
    }
    return hist;
}

string build_nominal_hist_name(
    const string& bin_name,
    const string& process,
    const string& era
) {
    return bin_name + "/" + process + "_" + era;
}

string build_systematic_hist_name(
    const string& bin_name,
    const string& process,
    const string& systematic,
    const string& direction,
    const string& era
) {
    return bin_name + "/" + process + "_" + systematic + direction + "_" + era;
}

double round_to_significant_digits(double value, int digits) {
    if (value == 0.0 || !std::isfinite(value)) return value;

    double scale = std::pow(10.0, digits - 1 - std::floor(std::log10(std::fabs(value))));
    return std::round(value * scale) / scale;
}

double round_to_fixed_digits(double value, int digits) {
    if (value == 0.0 || !std::isfinite(value)) return value;

    double scale = std::pow(10.0, digits);
    return std::round(value * scale) / scale;
}

bool should_use_asymmetric_lnn(double down_ratio, double up_ratio) {
    return std::fabs(down_ratio - 1.0 / up_ratio) >= 0.01;
}

double get_symmetric_lnn_value(double down_ratio, double up_ratio) {
    return std::sqrt(up_ratio / down_ratio);
}

bool get_sr_lnN_ratios(
    TFile* file,
    const string& bin_name,
    const string& process,
    const string& systematic,
    const string& era,
    double& down_ratio,
    double& up_ratio
) {
    TH1* nominal = get_histogram(file, build_nominal_hist_name(bin_name, process, era));
    TH1* hist_down = get_histogram(
        file, build_systematic_hist_name(bin_name, process, systematic, "Down", era)
    );
    TH1* hist_up = get_histogram(
        file, build_systematic_hist_name(bin_name, process, systematic, "Up", era)
    );

    if (!nominal || !hist_down || !hist_up) return false;

    double nominal_yield = nominal->GetBinContent(1);
    if (!(nominal_yield > 0.0) || !std::isfinite(nominal_yield)) {
        cerr << "Warning: Invalid nominal SR yield for process " << process
             << " in bin " << bin_name << " for era " << era
             << ". Skipping lnN conversion for systematic " << systematic << endl;
        return false;
    }

    down_ratio = hist_down->GetBinContent(1) / nominal_yield;
    if (!(down_ratio > 0.0)) {
        cerr << "Warning: Invalid SR down ratio for process " << process
             << " in bin " << bin_name << " for era " << era
             << " and systematic " << systematic 
             << ". Will use dummy value 0.1." << endl;
        down_ratio = 0.1;
    }
    up_ratio = hist_up->GetBinContent(1) / nominal_yield;
    if (!(down_ratio > 0.0)) {
        cerr << "Warning: Invalid SR up ratio for process " << process
             << " in bin " << bin_name << " for era " << era
             << " and systematic " << systematic 
             << ". Will use dummy value 0.1." << endl;
        up_ratio = 0.1;
    }
    if (!(down_ratio > 0.0) || !(up_ratio > 0.0) ||
        !std::isfinite(down_ratio) || !std::isfinite(up_ratio)) {
        cerr << "Warning: Invalid SR ratios for process " << process
             << " in bin " << bin_name << " for era " << era
             << " and systematic " << systematic << ". Skipping." << endl;
        return false;
    }

    return true;
}

bool get_sr_gmn_parameters(
    TFile* file,
    const string& bin_name,
    const string& process,
    const string& era,
    double& effective_events,
    int& n_events,
    double& alpha,
    double& lnn_value
) {
    TH1* nominal = get_histogram(file, build_nominal_hist_name(bin_name, process, era));
    if (!nominal) return false;

    double yield = nominal->GetBinContent(1);
    double error = nominal->GetBinError(1);
    if (!(yield > 0.0) || !(error > 0.0) || !std::isfinite(yield) || !std::isfinite(error)) {
        cerr << "Warning: Invalid SR gmN inputs for process " << process
             << " in bin " << bin_name << " for era " << era
             << " (yield=" << yield << ", error=" << error << "). Skipping." << endl;
        return false;
    }

    effective_events = std::pow(yield / error, 2);
    long rounded_events = std::lround(effective_events);
    if (!(rounded_events >= 1.0) || !std::isfinite(effective_events)) {
        cerr << "Warning: Invalid effective event count for process " << process
             << " in bin " << bin_name << " for era " << era
             << " (N_eff=" << effective_events << "). Skipping." << endl;
        return false;
    }

    if (rounded_events < 1 ||
        rounded_events > static_cast<long>(std::numeric_limits<int>::max())) {
        cerr << "Warning: Effective event count out of range for process " << process
             << " in bin " << bin_name << " for era " << era
             << " (N_eff=" << effective_events << "). Skipping." << endl;
        return false;
    }

    n_events = static_cast<int>(rounded_events);
    alpha = yield / static_cast<double>(n_events);
    lnn_value = (yield + error) / yield;
    if (!(alpha > 0.0) || !std::isfinite(alpha)) {
        cerr << "Warning: Invalid gmN alpha for process " << process
             << " in bin " << bin_name << " for era " << era
             << " (alpha=" << alpha << "). Skipping." << endl;
        return false;
    }
    if (!(lnn_value > 0.0) || !std::isfinite(lnn_value)) {
        cerr << "Warning: Invalid lnN stat value for process " << process
             << " in bin " << bin_name << " for era " << era
             << " (lnN=" << lnn_value << "). Skipping." << endl;
        return false;
    }

    return true;
}

double get_histogram_integral(TFile* file, const string& bin_name, const string& process, const string& era) {
    if (!file || file->IsZombie()) return 0.0;

    string hist_name = bin_name + "/" + process + "_" + era;
    TH1* hist = dynamic_cast<TH1*>(file->Get(hist_name.c_str()));
    if (!hist) {
        cerr << "Warning: Histogram " << hist_name << " does not exist in file: "
             << file->GetName() << "\tTreating yield as zero." << endl;
        return 0.0;
    }
    return hist->Integral();
}

vector<string> select_backgrounds_for_bin(
    TFile* file,
    const string& era,
    const string& bin_name,
    const vector<string>& candidate_backgrounds,
    const bool debug = false
) {
    constexpr double kMinYield = 1e-4;
    constexpr double kKeepYield = 1.0;
    constexpr double kMinFraction = 1e-2;
    const bool is_control_region =
        bin_name.size() >= 3 && bin_name.compare(0, 3, "CR_") == 0;

    vector<pair<string, double>> yields;
    yields.reserve(candidate_backgrounds.size());

    double total_background_yield = 0.0;
    for (const auto& process : candidate_backgrounds) {
        double yield = get_histogram_integral(file, bin_name, process, era);
        yields.emplace_back(process, yield);
        total_background_yield += yield;
    }

    vector<string> kept_backgrounds;
    kept_backgrounds.reserve(candidate_backgrounds.size());

    for (const auto& process_yield : yields) {
        const string& process = process_yield.first;
        double yield = process_yield.second;
        double fractional_contribution =
            total_background_yield > 0.0 ? yield / total_background_yield : 0.0;
        const bool yield_above_keep_threshold = yield > kKeepYield;
        const bool yield_above_min = yield > kMinYield;
        const bool fraction_above_min = fractional_contribution > kMinFraction;
        const bool is_sr_dy_or_qcd =
            !is_control_region && (process == "DY" || process == "QCD");
        const bool keep_process =
            yield_above_keep_threshold ||
            (is_control_region
                ? (yield_above_min && fraction_above_min)
                : (is_sr_dy_or_qcd || (yield_above_min && fraction_above_min)));
        const bool drop_process = !keep_process;

        if (drop_process) {
            if (debug) {
                cout << "Pruning background " << process
                    << " from bin " << bin_name
                    << " in era " << era
                    << " with yield " << yield
                    << " and fractional contribution " << fractional_contribution
                    << endl;
            }
            continue;
        }

        kept_backgrounds.push_back(process);
    }

    return kept_backgrounds;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        cout << "Usage: " << argv[0] 
        << " <input root file>" << " <signal list txt file>" 
        << " <signal region (SR_high_temp or SR_low_temp)>" 
        << " <list of years to include>" << endl;
        return 1;
    }

    vector<string> years;
    for (int i = 4; i < argc; ++i) {
        string year(argv[i]);
        years.push_back(year);
    }
    
    // Load input file
    string input_file(argv[1]);
    string filename = get_filename(input_file);
    string aux = string(getenv("CMSSW_BASE")) + "/src/auxiliaries/";

    // Open the input files and check if they exist
    vector<TFile*> rootfiles(years.size());
    for (size_t i = 0; i < years.size(); ++i) {
        string filename_i = input_file;
        size_t pos = filename_i.find(".root");
        if (pos != string::npos) {
            filename_i.replace(pos, 5, "_" + years[i] + ".root");
        } else {
            cerr << "Error: input file name does not contain '.root': " << filename_i << endl;
            return 1;
        }
        cout << "Opening input file: " << filename_i << endl;
        rootfiles[i] = TFile::Open(filename_i.c_str());
        if (!rootfiles[i] || rootfiles[i]->IsZombie()) {
            cerr << "Error: Unable to open input file: " << filename_i << endl;
            return 1;
        }
    }

    // Load signal list
    string signal_list(argv[2]);
    ifstream file(signal_list);
    if (!file.is_open()) {
        cerr << "Error: Unable to open signal list file" << endl;
        return 1;
    }
    vector<string> signals;
    string line;
    while (getline(file, line)) {
        signals.push_back(line);
    }
    file.close();

    // Load signal region
    string signal_region(argv[3]);
    if (signal_region != "SR_high_temp" && signal_region != "SR_low_temp") {
        cerr << "Error: Invalid signal region" << endl;
        return 1;
    }

    string com_energy = "";
    for (auto signal : signals) {
        ch::CombineHarvester cb;
        vector<string> bkg_procs = {
            "QCD", "DY", "TT", "ST", "VV+VVV", "WJets", "TTV", "Higgs"
        };
        vector<string> ps_bkg_procs = {
            "DY", "TT", "ST", "VV+VVV", "WJets", "TTV", "Higgs"
        };
        vector<vector<string>> sr_bkg_procs_by_year(years.size());
        vector<vector<string>> sr_ps_bkg_procs_by_year(years.size());

        for (size_t i = 0; i < years.size(); ++i) {
            string era = get_com_energy(years[i]) + "_" + years[i];

            // Check if the histogram exists in the input file
            string hist_name = "CR_QCD_" + era + "/" + signal + "_" + era;
            if (!check_histogram_exists(rootfiles[i], hist_name)) {
                cerr << "Warning: Histogram " << hist_name << " does not exist in file: " 
                     << rootfiles[i]->GetName() << "\tSkipping..." << endl;
                continue;
            }
            
            ch::Categories cats = {
                {1, string("CR_QCD") + string("_") + era}, 
                {2, string("CR_DY") + string("_") + era}, 
                {3, signal_region + "_" + era}
            };

            // Observations
            cb.AddObservations({"*"}, {"SUEP"}, {era}, {"muon"}, cats);

            // Bkg processes
            for (const auto& cat : cats) {
                vector<string> kept_backgrounds = select_backgrounds_for_bin(
                    rootfiles[i], era, cat.second, bkg_procs
                );
                if (kept_backgrounds.empty()) {
                    cout << "No backgrounds retained for bin " << cat.second
                         << " in era " << era << endl;
                    continue;
                }
                cb.AddProcesses(
                    {"*"}, {"SUEP"}, {era}, {"muon"}, kept_backgrounds, {cat}, false
                );
            }

            // Sig process
            cb.AddProcesses(
                {"*"}, {"SUEP"}, {era}, {"muon"}, {signal}, cats, true
            );

            sr_bkg_procs_by_year[i] = select_backgrounds_for_bin(
                rootfiles[i], era, signal_region + "_" + era, bkg_procs
            );
            for (const auto& process : ps_bkg_procs) {
                if (find(sr_bkg_procs_by_year[i].begin(), sr_bkg_procs_by_year[i].end(), process) !=
                    sr_bkg_procs_by_year[i].end()) {
                    sr_ps_bkg_procs_by_year[i].push_back(process);
                }
            }
        }

        // Add scaling systematics
        cb.cp().backgrounds().AddSyst(
            cb, 
            "lumi_2016", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init({"13TeV_2016"}, 1.01)
        );
        cb.cp().signals().AddSyst(
            cb, 
            "lumi_2016", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init({"13TeV_2016"}, 1.01)
        );
        cb.cp().backgrounds().AddSyst(
            cb, 
            "lumi_2017", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init
                ({"13TeV_2017"}, 1.02)
        );
        cb.cp().signals().AddSyst(
            cb, 
            "lumi_2017", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init
                ({"13TeV_2017"}, 1.02)
        );
        cb.cp().backgrounds().AddSyst(
            cb, 
            "lumi_2018", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init({"13TeV_2018"}, 1.015)
        );
        cb.cp().signals().AddSyst(
            cb, 
            "lumi_2018", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init({"13TeV_2018"}, 1.015)
        );
        cb.cp().backgrounds().AddSyst(
            cb, 
            "lumi_13TeV_correlated", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init
                ({"13TeV_2016"}, 1.006)
                ({"13TeV_2017"}, 1.009)
                ({"13TeV_2018"}, 1.02)
        );
        cb.cp().signals().AddSyst(
            cb, 
            "lumi_13TeV_correlated", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init
                ({"13TeV_2016"}, 1.006)
                ({"13TeV_2017"}, 1.009)
                ({"13TeV_2018"}, 1.02)
        );
        cb.cp().backgrounds().AddSyst(
            cb, 
            "lumi_13TeV_1718", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init
                ({"13TeV_2017"}, 1.006)
                ({"13TeV_2018"}, 1.002)
        );
        cb.cp().signals().AddSyst(
            cb, 
            "lumi_13TeV_1718", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init
                ({"13TeV_2017"}, 1.006)
                ({"13TeV_2018"}, 1.002)
        );

        auto add_sr_background_lnN = [&](TFile* file, const string& era, const string& systematic,
                                         const vector<string>& processes) {
            string sr_bin = signal_region + "_" + era;
            for (const auto& process : processes) {
                double down_ratio = 0.0;
                double up_ratio = 0.0;
                if (!get_sr_lnN_ratios(file, sr_bin, process, systematic, era, down_ratio, up_ratio)) {
                    continue;
                }
                if (should_use_asymmetric_lnn(down_ratio, up_ratio)) {
                    cb.cp().backgrounds().bin({sr_bin}).era({era}).process({process}).AddSyst(
                        cb,
                        systematic,
                        "lnN",
                        ch::syst::SystMapAsymm<>::init(
                            round_to_fixed_digits(down_ratio, 3),
                            round_to_fixed_digits(up_ratio, 3)
                        )
                    );
                } else {
                    cb.cp().backgrounds().bin({sr_bin}).era({era}).process({process}).AddSyst(
                        cb,
                        systematic,
                        "lnN",
                        ch::syst::SystMap<>::init(
                            round_to_fixed_digits(get_symmetric_lnn_value(down_ratio, up_ratio), 3)
                        )
                    );
                }
            }
        };

        auto add_sr_signal_lnN = [&](TFile* file, const string& era, const string& systematic) {
            string sr_bin = signal_region + "_" + era;
            double down_ratio = 0.0;
            double up_ratio = 0.0;
            if (!get_sr_lnN_ratios(file, sr_bin, signal, systematic, era, down_ratio, up_ratio)) {
                return;
            }
            if (should_use_asymmetric_lnn(down_ratio, up_ratio)) {
                cb.cp().signals().bin({sr_bin}).era({era}).process({signal}).AddSyst(
                    cb,
                    systematic,
                    "lnN",
                    ch::syst::SystMapAsymm<>::init(
                        round_to_fixed_digits(down_ratio, 3),
                        round_to_fixed_digits(up_ratio, 3)
                    )
                );
            } else {
                cb.cp().signals().bin({sr_bin}).era({era}).process({signal}).AddSyst(
                    cb,
                    systematic,
                    "lnN",
                    ch::syst::SystMap<>::init(
                        round_to_fixed_digits(get_symmetric_lnn_value(down_ratio, up_ratio), 3)
                    )
                );
            }
        };

        // Add shape systematics
        // Apply the common correlated systematics to all processes
        vector<string> common_shape_corr_systematics = {
            "PUReweight_13TeV",
            "LHEPdf_13TeV",
            "LHEScaleMuF_13TeV",
            "LHEScaleMuR_13TeV"
        };
        for (size_t i = 0; i < years.size(); ++i) {
            string era = get_com_energy(years[i]) + "_" + years[i];
            vector<string> cr_bins = {"CR_QCD_" + era, "CR_DY_" + era};
            for (const auto& syst : common_shape_corr_systematics) {
                cb.cp().backgrounds().bin(cr_bins).era({era}).AddSyst(
                    cb,
                    syst,
                    "shape",
                    ch::syst::SystMap<>::init(1.0)
                );
                cb.cp().signals().bin(cr_bins).era({era}).AddSyst(
                    cb,
                    syst,
                    "shape",
                    ch::syst::SystMap<>::init(1.0)
                );
                add_sr_background_lnN(rootfiles[i], era, syst, sr_bkg_procs_by_year[i]);
                add_sr_signal_lnN(rootfiles[i], era, syst);
            }
        }

        // Apply the common uncorrelated systematics to all processes
        vector<string> common_shape_uncorr_systematics = {"TrigSF", "L1PreFire", "MuonSF"};
        for (size_t i = 0; i < years.size(); ++i) {
            string era = get_com_energy(years[i]) + "_" + years[i];
            vector<string> cr_bins = {"CR_QCD_" + era, "CR_DY_" + era};
            for (const auto& syst_base : common_shape_uncorr_systematics) {
                string syst = syst_base + "_" + era;
                cb.cp().backgrounds().bin(cr_bins).era({era}).AddSyst(
                    cb,
                    syst,
                    "shape",
                    ch::syst::SystMap<>::init(1.0)
                );
                cb.cp().signals().bin(cr_bins).era({era}).AddSyst(
                    cb,
                    syst,
                    "shape",
                    ch::syst::SystMap<>::init(1.0)
                );
                add_sr_background_lnN(rootfiles[i], era, syst, sr_bkg_procs_by_year[i]);
                add_sr_signal_lnN(rootfiles[i], era, syst);
            }
        }

        // Need to add this in a separate step for now until I figure out why TrkEff is missing in some cases
        if (signal_region == "SR_low_temp") {
            for (size_t i = 0; i < years.size(); ++i) {
                string era = get_com_energy(years[i]) + "_" + years[i];
                string syst = "TrkEff_" + era;

                // Check if the histogram exists in the input file
                string hist_name = build_systematic_hist_name(
                    "SR_low_temp_" + era, signal, syst, "Up", era
                );
                if (!check_histogram_exists(rootfiles[i], hist_name)) {
                    cerr << "Warning: Histogram " << hist_name << " does not exist in file: " 
                        << rootfiles[i]->GetName() << "\tSkipping..." << endl;
                    continue;
                }

                add_sr_background_lnN(rootfiles[i], era, syst, sr_bkg_procs_by_year[i]);
                add_sr_signal_lnN(rootfiles[i], era, syst);
            }
        }
        
        // Apply PS systematics to all samples except QCD
        vector<string> PS_shape_systematics = {"ISR_13TeV", "FSR_13TeV"};
        for (size_t i = 0; i < years.size(); ++i) {
            string era = get_com_energy(years[i]) + "_" + years[i];
            vector<string> cr_bins = {"CR_QCD_" + era, "CR_DY_" + era};
            for (const auto& syst : PS_shape_systematics) {
                cb.cp().backgrounds().bin(cr_bins).era({era}).process(ps_bkg_procs).AddSyst(
                    cb,
                    syst,
                    "shape",
                    ch::syst::SystMap<>::init(1.0)
                );
                cb.cp().signals().bin(cr_bins).era({era}).AddSyst(
                    cb,
                    syst,
                    "shape",
                    ch::syst::SystMap<>::init(1.0)
                );
                add_sr_background_lnN(rootfiles[i], era, syst, sr_ps_bkg_procs_by_year[i]);
                add_sr_signal_lnN(rootfiles[i], era, syst);
            }
        }

        // Manual MCStats
        vector<string> manual_MC_stats = {
            "SRLowTBin0",
            "SRHighTBin0"
        };
        for (size_t i = 0; i < years.size(); ++i) {
            string era = get_com_energy(years[i]) + "_" + years[i];
            for (auto const& unc : manual_MC_stats) {
                string target_bin;
                if (unc.compare(0, 6, "SRLowT") == 0 && signal_region == "SR_low_temp") {
                    target_bin = signal_region + "_" + era;
                } else if (unc.compare(0, 7, "SRHighT") == 0 && signal_region == "SR_high_temp") {
                    target_bin = signal_region + "_" + era;
                } else {
                    continue;
                }

                for (const auto& process : sr_bkg_procs_by_year[i]) {
                    double effective_events = 0.0;
                    int n_events = 0;
                    double alpha = 0.0;
                    double lnn_value = 0.0;
                    if (!get_sr_gmn_parameters(
                        rootfiles[i], target_bin, process, era, effective_events, n_events, alpha, lnn_value
                    )) {
                        continue;
                    }
                    if (effective_events >= 10.0) {
                        cb.cp().backgrounds().bin({target_bin}).era({era}).process({process}).AddSyst(
                            cb,
                            string("MCStat") + process + unc + "_" + era,
                            "lnN",
                            ch::syst::SystMap<>::init(round_to_fixed_digits(lnn_value, 3))
                        );
                    } else {
                        cb.cp().backgrounds().bin({target_bin}).era({era}).process({process}).AddSyst(
                            cb,
                            string("MCStat") + process + unc + "_" + era,
                            "gmN",
                            ch::syst::SystMapGmN<>::init(
                                n_events, round_to_significant_digits(alpha, 3)
                            )
                        );
                    }
                }

                double signal_effective_events = 0.0;
                int signal_n_events = 0;
                double signal_alpha = 0.0;
                double signal_lnn_value = 0.0;
                if (!get_sr_gmn_parameters(rootfiles[i], target_bin, signal, era,
                                           signal_effective_events, signal_n_events,
                                           signal_alpha, signal_lnn_value)) {
                    continue;
                }
                if (signal_effective_events >= 10.0) {
                    cb.cp().signals().bin({target_bin}).era({era}).process({signal}).AddSyst(
                        cb,
                        string("MCStatSUEP") + unc + "_" + era,
                        "lnN",
                        ch::syst::SystMap<>::init(round_to_fixed_digits(signal_lnn_value, 3))
                    );
                } else {
                    cb.cp().signals().bin({target_bin}).era({era}).process({signal}).AddSyst(
                        cb,
                        string("MCStatSUEP") + unc + "_" + era,
                        "gmN",
                        ch::syst::SystMapGmN<>::init(
                            signal_n_events, round_to_significant_digits(signal_alpha, 3)
                        )
                    );
                }
            }
        }

        // Add rateParam's 
        cb.cp().backgrounds().process({"QCD"}).AddSyst(
            cb, "rate_QCD_13TeV", "rateParam", ch::syst::SystMap<>::init(1.0)
        );
        cb.cp().backgrounds().process({"DY"}).AddSyst(
            cb, "rate_DY_13TeV", "rateParam", ch::syst::SystMap<>::init(1.0)
        );
        
        for (size_t i = 0; i < years.size(); ++i) {
            string era = get_com_energy(years[i]) + "_" + years[i];

            // Check if the histogram exists in the input file
            string hist_name = "CR_QCD_" + era + "/" + signal + "_" + era;
            if (!check_histogram_exists(rootfiles[i], hist_name)) {
                cerr << "Warning: Histogram " << hist_name << " does not exist in file: " 
                     << rootfiles[i]->GetName() << "\tSkipping..." << endl;
                continue;
            }

            cb.cp().backgrounds().era({era}).ExtractShapes(
                aux + "input/" + filename + "_" + years[i] + ".root",
                "$BIN/$PROCESS_" + era,
                "$BIN/$PROCESS_$SYSTEMATIC_" + era
            );
            cb.cp().signals().era({era}).ExtractShapes(
                aux + "input/" + filename + "_" + years[i] + ".root",
                "$BIN/$PROCESS_" + era,
                "$BIN/$PROCESS_$SYSTEMATIC_" + era
            );

            // AutoMCStats for CRs
            cb.cp().bin({"CR_QCD_" + era, "CR_DY_" + era}).SetAutoMCStats(cb, 5);
        }

        suep::apply_systematic_name_map(cb);

        // Output file name
        string output_name = filename + "_" + signal;
                
        // Write datacard for this mass point
        TFile output((aux + "cards/" + output_name + "_Run2_13TeV.root").c_str(), "RECREATE");
        cb.WriteDatacard((aux + "cards/" + output_name + "_Run2_13TeV.txt"), output);
        output.Close();
        
        cout << ">> Created datacard for signal point: " << signal << endl;
    }
    
    return 0;
}
