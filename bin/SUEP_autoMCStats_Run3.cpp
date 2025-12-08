#include <iostream>
#include <fstream>
#include <iomanip>
#include <array>
#include <vector>
#include <string>
#include <tuple>
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
            vector<string> bkg_procs = {
                "QCD", "DY", "TT", "VV+VVV", "WJets", "Higgs"
            };
            cb.AddProcesses(
                {"*"}, {"SUEP"}, {era}, {"muon"}, bkg_procs, cats, false
            );

            // Sig process
            cb.AddProcesses(
                {"*"}, {"SUEP"}, {era}, {"muon"}, {signal}, cats, true
            );
        }

        // Add scaling systematics
        cb.cp().backgrounds().era({"13p6TeV_2022", "13p6TeV_2022EE"}).AddSyst(
            cb, 
            "lumi_13p6TeV_2022", 
            "lnN", 
            ch::syst::SystMap<>::init(1.014)
        );
        cb.cp().signals().era({"13p6TeV_2022", "13p6TeV_2022EE"}).AddSyst(
            cb, 
            "lumi_13p6TeV_2022", 
            "lnN", 
            ch::syst::SystMap<>::init(1.014)
        );
        cb.cp().backgrounds().era({"13p6TeV_2023", "13p6TeV_2023BPix"}).AddSyst(
            cb, 
            "lumi_13p6TeV_2023", 
            "lnN", 
            ch::syst::SystMap<>::init(1.013)
        );
        cb.cp().signals().era({"13p6TeV_2023", "13p6TeV_2023BPix"}).AddSyst(
            cb, 
            "lumi_13p6TeV_2023", 
            "lnN", 
            ch::syst::SystMap<>::init(1.013)
        );

        // Add shape systematics
        // Apply the common correlated systematics to all processes
        vector<string> common_shape_corr_systematics = {
            "PUReweight_13p6TeV",
            // "LHEPdf",
            "LHEScaleMuF_13p6TeV",
            "LHEScaleMuR_13p6TeV"
        };
        for (auto syst : common_shape_corr_systematics) {
            cb.cp().backgrounds().AddSyst(
                cb, 
                syst, 
                "shape", 
                ch::syst::SystMap<>::init(1.0)
            );
            cb.cp().signals().AddSyst(
                cb, 
                syst, 
                "shape", 
                ch::syst::SystMap<>::init(1.0)
            );
        }

        // Apply the common uncorrelated systematics to all processes
        vector<string> common_shape_uncorr_systematics = {"MuonSF"};
        for (auto syst : common_shape_uncorr_systematics) {
            cb.cp().backgrounds().AddSyst(
                cb, 
                syst + string("_$ERA"), 
                "shape", 
                ch::syst::SystMap<ch::syst::era>::init
                    ({"13p6TeV_2022"}, 1)
                    ({"13p6TeV_2022EE"}, 1)
                    ({"13p6TeV_2023"}, 1)
                    ({"13p6TeV_2023BPix"}, 1)
            );
            cb.cp().signals().AddSyst(
                cb, 
                syst + string("_$ERA"), 
                "shape", 
                ch::syst::SystMap<ch::syst::era>::init
                    ({"13p6TeV_2022"}, 1)
                    ({"13p6TeV_2022EE"}, 1)
                    ({"13p6TeV_2023"}, 1)
                    ({"13p6TeV_2023BPix"}, 1)
            );
        }

        // Need to add this in a separate step for now until I figure out why TrkEff is missing in some cases
        if (signal_region == "SR_low_temp") {
            for (size_t i = 0; i < years.size(); ++i) {
                string era = get_com_energy(years[i]) + "_" + years[i];

                // Check if the histogram exists in the input file
                string hist_name = "SR_low_temp_" + era + "/" + signal + "_TrkEff_" + era + "Up_" + era;
                if (!check_histogram_exists(rootfiles[i], hist_name)) {
                    cerr << "Warning: Histogram " << hist_name << " does not exist in file: " 
                        << rootfiles[i]->GetName() << "\tSkipping..." << endl;
                    continue;
                }

                cb.cp().backgrounds().bin({signal_region + "_" + era}).era({era}).AddSyst(
                    cb, 
                    "TrkEff_" + era, 
                    "shape", 
                    ch::syst::SystMap<>::init(1.0)
                );
                cb.cp().signals().bin({signal_region + "_" + era}).era({era}).AddSyst(
                    cb, 
                    "TrkEff_" + era,
                    "shape", 
                    ch::syst::SystMap<>::init(1.0)
                );
            }
        }

        // Apply PS systematics to all samples except QCD
        vector<string> PS_shape_systematics = {
            "ISR_13p6TeV",
            "FSR_13p6TeV"
        };
        for (auto syst : PS_shape_systematics) {
            cb.cp().backgrounds().process(
                {"DY", "TT", "VV+VVV", "WJets", "Higgs"}
            ).AddSyst(
                cb, 
                syst, 
                "shape", 
                ch::syst::SystMap<>::init(1.0)
            );
            cb.cp().signals().AddSyst(
                cb, 
                syst, 
                "shape", 
                ch::syst::SystMap<>::init(1.0)
            );
        }

        // Add rateParam's 
        cb.cp().backgrounds().process({"QCD"}).AddSyst(
            cb, "k_QCD_13p6TeV", "rateParam", ch::syst::SystMap<>::init(1.0)
        );
        cb.cp().backgrounds().process({"DY"}).AddSyst(
            cb, "k_DY_13p6TeV", "rateParam", ch::syst::SystMap<>::init(1.0)
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
            cb.cp().bin({signal_region + string("_") + era}).SetAutoMCStats(cb, 0);
        }

        // Output file name
        string output_name = filename + "_" + signal;
                
        // Write datacard for this mass point
        TFile output((aux + "cards/" + output_name + "_Run3_13p6TeV.root").c_str(), "RECREATE");
        cb.WriteDatacard((aux + "cards/" + output_name + "_Run3_13p6TeV.txt"), output);
        output.Close();
        
        cout << ">> Created datacard for signal point: " << signal << endl;
    }
    
    return 0;
}
