# Create datacards
SUEP_autoMCStats_Run2 auxiliaries/input/SUEP_signal_scale0.001.root auxiliaries/input/models_for_high_temp.txt SR_high_temp 2016 2017 2018
SUEP_autoMCStats_Run3 auxiliaries/input/SUEP_signal_scale0.001.root auxiliaries/input/models_for_high_temp.txt SR_high_temp 2022 2022EE 2023 2023BPix

# Combine datacards
combineCards.py run2_13TeV=auxiliaries/cards/SUEP_signal_scale0.001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic_Run2_13TeV.txt \
    run3_13p6TeV=auxiliaries/cards/SUEP_signal_scale0.001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic_Run3_13p6TeV.txt \
    > auxiliaries/cards/SUEP_signal_scale0.001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.txt

# Validate datacards
ValidateDatacards.py -p 3 auxiliaries/cards/SUEP_signal_scale0.001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.txt

# To get text2workspace.py to work
export PYTHONNOUSERSITE=1

# Initial setup
text2workspace.py auxiliaries/cards/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic_Run2_13TeV.txt \
    -o auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic_Run2_13TeV.root
text2workspace.py auxiliaries/cards/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic_Run3_13p6TeV.txt \
    -o auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic_Run3_13p6TeV.root
text2workspace.py auxiliaries/cards/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.txt \
    -o auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root

# Do CLs
combine -M AsymptoticLimits auxiliaries/workspaces/SUEP_signal_scale0.001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root \
    -m 125 --rMin -5 --rMax 20 --run blind

# Run with toys
combine -M HybridNew auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root \
    -H AsymptoticLimits --name toys_limit --LHCmode LHC-limits --cl 0.95 --rAbsAcc 0.00001 --rRelAcc 0.01 --expectedFromGrid=0.5 \
    --mass 125 --adaptiveToys 1 -T 1500 --fullBToys --rMin -5 --rMax 20 -v 1 --plot=limit_scan.png

# diffNuisances
combine -M FitDiagnostics auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root \
    -m 125 --rMin -5 --rMax 20 --robustFit 1 --cminDefaultMinimizerStrategy 0 -t -1 --expectSignal 0
python HiggsAnalysis/CombinedLimit/test/diffNuisances.py -a fitDiagnosticsTest.root -g plots.root

# Do FitDiagnostics
combine -M FitDiagnostics auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root -m 125 \
    --rMin -5 --rMax 20 --saveShapes --saveWithUncertainties -n .SUEP --robustFit 1 --cminDefaultMinimizerStrategy 0 -t -1 --expectSignal 0 --plots
PostFitShapesFromWorkspace -d auxiliaries/cards/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.txt \
    -w auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root --output postfit_plots.root \
    --total-shapes --postfit --sampling --covariance -f fitDiagnostics.SUEP.root:fit_b --print

# Do impacts
combineTool.py -M Impacts -d auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root -m 125 \
    --rMin -5 --rMax 20 --setParameters k_QCD_13TeV=0.7,k_DY_13TeV=1.0 --robustFit 1 --cminDefaultMinimizerStrategy 0 -t -1 --expectSignal 0 --doInitialFit
combineTool.py -M Impacts -d auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root -m 125 \
    --rMin -5 --rMax 20 --setParameters k_QCD_13TeV=0.7,k_DY_13TeV=1.0 --robustFit 1 --cminDefaultMinimizerStrategy 0 -t -1 --expectSignal 0 --doFits
combineTool.py -M Impacts -d auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS125.000_mPhi8.000_T32.000_modeleptonic.root -m 125 \
    --rMin -5 --rMax 20 --setParameters k_QCD_13TeV=0.7,k_DY_13TeV=1.0 --robustFit 1 --cminDefaultMinimizerStrategy 0 -t -1 --expectSignal 0 --output impacts.json
plotImpacts.py -i impacts.json -o impacts
