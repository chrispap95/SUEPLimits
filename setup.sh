#!/bin/sh

CMSSW_VERSION=CMSSW_14_1_0_pre4

# Setup CMSSW environment
cmsrel $CMSSW_VERSION
cd $CMSSW_VERSION/src
cmsenv

# Clone Combine
git -c advice.detachedHead=false clone --depth 1 --branch v10.4.1 https://github.com/cms-analysis/HiggsAnalysis-CombinedLimit.git HiggsAnalysis/CombinedLimit

# Clone CombineHarvester
git clone --branch v3.0.0 https://github.com/cms-analysis/CombineHarvester.git CombineHarvester

# Clone SUEPLimits
git clone git@github.com:chrispap95/SUEPLimits.git SUEPLimits/SUEPLimits

# Prepare workspace
mkdir -pv auxiliaries/{input,cards,workspaces}
cp SUEPLimits/SUEPLimits/run*.sh $CMSSW_BASE/src/

# Build
scram b clean
scram b -j$(nproc --ignore=2) # always make a clean build, with n - 2 cores on the system
