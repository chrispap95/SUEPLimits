#!/bin/bash -e
max_jobs=48  # Adjust as needed

# Step 1: Create datacards for Run 2 and Run 3 and combine them
SUEP_autoMCStats_Run2 auxiliaries/input/SUEP_signal_scale0.0001.root auxiliaries/input/models_for_high_temp.txt SR_high_temp 2016 2017 2018
SUEP_autoMCStats_Run2 auxiliaries/input/SUEP_signal_scale0.0001.root auxiliaries/input/models_for_low_temp.txt SR_low_temp 2016 2017 2018
SUEP_autoMCStats_Run3 auxiliaries/input/SUEP_signal_scale0.0001.root auxiliaries/input/models_for_high_temp.txt SR_high_temp 2022 2022EE 2023 2023BPix
SUEP_autoMCStats_Run3 auxiliaries/input/SUEP_signal_scale0.0001.root auxiliaries/input/models_for_low_temp.txt SR_low_temp 2022 2022EE 2023 2023BPix
for model in auxiliaries/cards/SUEP_signal*Run2_13TeV.txt; do
    combineCards.py run2_13TeV=$model run3_13p6TeV=${model/Run2_13TeV/Run3_13p6TeV} > ${model/_Run2_13TeV/}
done

# Step 2: create workspaces
job_count=0
for scan_point in auxiliaries/cards/SUEP_signal_scale0.0001_GluGluToSUEP_mS*nic.txt; do 
    (
        name=$(basename $scan_point .txt)
        echo "Processing scan point: " $name
        text2workspace.py auxiliaries/cards/${name}.txt -o auxiliaries/workspaces/${name}.root
    ) & 
    ((job_count++))
    if (( job_count >= max_jobs )); then
        wait  # Wait when reaching the limit
        job_count=0
    fi
done 
wait

# Step 3: Iterate over workspace files
for file in auxiliaries/workspaces/SUEP_signal_scale0.0001_GluGluToSUEP_mS*nic.root; do
    # (
        # Extract parameters from filename
        filename=$(basename "$file" .root)
        mS=$(echo "$filename" | grep -oP 'mS\K[\d\.]+')
        mPhi=$(echo "$filename" | grep -oP 'mPhi\K[\d\.]+')
        T=$(echo "$filename" | grep -oP 'T\K[\d\.]+')
        mode=$(echo "$filename" | grep -oP 'mode\K\w+')

        echo "Running combine for: mS=$mS, mPhi=$mPhi, T=$T, mode=$mode"

        # Run the combine command
        combine "$file" -M AsymptoticLimits --mass "$mS" --cl 0.95 \
            --name "_scale0.0001_mPhi${mPhi}_T${T}_${mode}" \
            --rAbsAcc 0.00001 --rRelAcc 0.01 --run blind -v 1
    # ) &
    # ((job_count++))
    # if (( job_count >= max_jobs )); then
    #     wait  # Wait when reaching the limit
    #     job_count=0
    # fi
done
wait

# Step 4: Merge results by common mPhi, T, and mode
for mPhi in $(ls auxiliaries/workspaces/ | grep -oP 'mPhi\K[\d\.]+' | sort -u); do
    for T in $(ls auxiliaries/workspaces/ | grep -oP 'T\K[\d\.]+' | sort -u); do
        for mode in hadronic leptonic; do
            output="higgsCombine_scale0.0001_mPhi${mPhi}_T${T}_${mode}.AsymptoticLimits"
            if ls ${output}.mH*.root 1> /dev/null 2>&1; then
                echo "Merging files for: mPhi=${mPhi}, T=${T}, mode=${mode}"
                hadd -f "${output}.root" ${output}.mH*.root
            fi
        done
    done
done
