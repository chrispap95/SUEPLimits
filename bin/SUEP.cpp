#include <iostream>
#include <fstream>
#include <iomanip>
#include <array>
#include <vector>
#include <string>
#include <tuple>
#include <cmath>
#include "CombineHarvester/CombineTools/interface/CombineHarvester.h"
#include "CombineHarvester/CombineTools/interface/Observation.h"
#include "CombineHarvester/CombineTools/interface/Process.h"
#include "CombineHarvester/CombineTools/interface/Utilities.h"
#include "CombineHarvester/CombineTools/interface/Systematics.h"
#include "CombineHarvester/CombineTools/interface/BinByBin.h"

#include "TFile.h"
#include "TH1.h"

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
    TFile* rootfiles[years.size()];
    for (size_t i = 0; i < years.size(); ++i) {
        string filename_i = input_file;
        filename_i.replace(filename_i.find(".root"), filename_i.length(), "_" + years[i] + ".root");

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
        ch::Categories cats = {{1, "CR_QCD"}, {2, "CR_DY"}, {3, signal_region}};

        for (size_t i = 0; i < years.size(); ++i) {
            if (years[i].substr(0, 3) == "201") {
                com_energy = "13TeV";
            } else if (years[i].substr(0, 3) == "202") {
                com_energy = "13p6TeV";
            } else {
                cerr << "Error: Invalid year format" << endl;
                return 1;
            }

            string era = com_energy + "_" + years[i];

            // Observations
            cb.AddObservations({"*"}, {"SUEP"}, {era}, {"muon"}, cats);

            // Bkg processes
            vector<string> bkg_procs = {
                "QCD", "DY", "TT", "ST", "VV+VVV", "WJets", "TTV", "Higgs"
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
        cb.cp().backgrounds().AddSyst(
            cb, 
            "lumi_$ERA", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init({era}, 1.025)
        );
        cb.cp().signals().AddSyst(
            cb, 
            "lumi_$ERA", 
            "lnN", 
            ch::syst::SystMap<ch::syst::era>::init({era}, 1.025)
        );

        // Add shape systematics
        // Apply the common systematics to all processes
        vector<string> common_shape_systematics = {
            "PUReweight",
            "L1PreFire",
            "MuonSF",
            "LHEScaleMuF",
            "LHEScaleMuR",
            "LHEPdf"
        };
        if (signal_region == "SR_low_temp") {
            common_shape_systematics.push_back("TrkEff");
        }
        for (auto syst : common_shape_systematics) {
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

        // Apply PS systematics to all samples except QCD
        vector<string> PS_shape_systematics = {
            "ISR",
            "FSR"
        };
        for (auto syst : PS_shape_systematics) {
            cb.cp().backgrounds().process(
                {"DY", "TT", "ST", "VV+VVV", "WJets", "TTV", "Higgs"}
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
            cb, "k_QCD", "rateParam", ch::syst::SystMap<>::init(1.0)
        );
        cb.cp().backgrounds().process({"DY"}).AddSyst(
            cb, "k_DY", "rateParam", ch::syst::SystMap<>::init(1.0)
        );

        // Add fit uncertainty for QCD and DY as a shape uncertainty for SR
        // cb.cp().bin({signal_region}).backgrounds().process({"QCD"}).AddSyst(
        //     cb, 
        //     "ExtrFitQCD", 
        //     "shape", 
        //     ch::syst::SystMap<>::init(1.0)
        // );
        // cb.cp().bin({signal_region}).backgrounds().process({"DY"}).AddSyst(
        //     cb, 
        //     "ExtrFitDY", 
        //     "shape", 
        //     ch::syst::SystMap<>::init(1.0)
        // );

        // Add signal statistical uncertainty in SR
        // This is supposedly a gmN uncertainty for low raw counts of MC events but they are not supported yet
        // auto hist = (TH1D*) ->Get((signal_region + "/" + signal).c_str());
        // if (!hist) {
        //     cerr << "Error: Unable to find histogram for signal: " << signal << endl;
        //     return 1;
        // }
        // double signal_yield = hist->GetBinContent(1);
        // double signal_error = hist->GetBinError(1);
        // double eff_MC_raw_evts = round(pow(signal_yield, 2) / pow(signal_error, 2));
        // double eff_MC_weight = signal_yield / eff_MC_raw_evts;
        // if (eff_MC_raw_evts < 100000 && eff_MC_raw_evts >= 1) {
        //     cb.cp().bin({signal_region}).signals().AddSyst(
        //         cb, 
        //         "signal_stat", 
        //         "gmN", 
        //         ch::syst::SystMap<>::init(unsigned(eff_MC_raw_evts))
        //     );  
        // } 
        // else {
        // cb.cp().bin({signal_region}).signals().AddSyst(
        //     cb, 
        //     "signal_stat", 
        //     "lnN", 
        //     ch::syst::SystMap<>::init((signal_yield + signal_error) / signal_yield)
        // );
        // }
        
        cb.cp().backgrounds().ExtractShapes(
            aux + "input/" + filename + "_" + years[i] + ".root",
            "$BIN/$PROCESS_" + era,
            "$BIN/$PROCESS_$SYSTEMATIC_" + era
        );
        cb.cp().signals().ExtractShapes(
            aux + "input/" + filename + "_" + years[i] + ".root",
            "$BIN/$PROCESS_" + era,
            "$BIN/$PROCESS_$SYSTEMATIC_" + era
        );

        // AutoMCStats for CRs
        cb.cp().bin({"CR_QCD", "CR_DY"}).SetAutoMCStats(cb, 5);
        cb.cp().bin({signal_region}).SetAutoMCStats(cb, 0);
        // Output file name
        string output_name = filename + "_" + signal;
                
        // Write datacard for this mass point
        TFile output((aux + "cards/" + output_name + ".root").c_str(), "RECREATE");
        cb.WriteDatacard((aux + "cards/" + output_name + ".txt"), output);
        output.Close();
        
        cout << ">> Created datacard for signal point: " << signal << endl;
    }

    // Close the input files
    for (size_t i = 0; i < years.size(); ++i) {
        if (rootfiles[i]) {
            rootfiles[i]->Close();
        }
    }
    
    return 0;
}
