#include <array>
#include <string>
#include <vector>

#include <TClonesArray.h>
#include <THistManager.h>
#include <THashList.h>

#include "AliAnalysisUtils.h"
#include "AliAnalysisTaskEGAMonitor.h"
#include "AliEMCALGeometry.h"
#include "AliEMCALTriggerPatchInfo.h"
#include "AliEMCALTriggerTypes.h"
#include "AliInputEventHandler.h"
#include "AliVVertex.h"
#include "AliVCaloTrigger.h"

/// \cond CLASSIMP
ClassImp(EMCalTriggerPtAnalysis::AliAnalysisTaskEGAMonitor)
/// \endcond

namespace EMCalTriggerPtAnalysis {

AliAnalysisTaskEGAMonitor::AliAnalysisTaskEGAMonitor() :
    AliAnalysisTaskEmcal(),
    fHistos(nullptr),
    fUseRecalcPatches(false),
    fRecalcLow(0.),
    fRecalcHigh(0.)
{
  this->SetNeedEmcalGeom(true);
  this->SetCaloTriggerPatchInfoName("EmcalTriggers");
}

AliAnalysisTaskEGAMonitor::AliAnalysisTaskEGAMonitor(const char *name) :
    AliAnalysisTaskEmcal(name, true),
    fHistos(nullptr),
    fUseRecalcPatches(false),
    fRecalcLow(0.),
    fRecalcHigh(0.)
{
  this->SetNeedEmcalGeom(true);
  this->SetCaloTriggerPatchInfoName("EmcalTriggers");
}

AliAnalysisTaskEGAMonitor::~AliAnalysisTaskEGAMonitor() {
  if(fHistos) delete fHistos;
  if(fGeom) delete fGeom;
}

void AliAnalysisTaskEGAMonitor::UserCreateOutputObjects(){
  fAliAnalysisUtils = new AliAnalysisUtils;

  fHistos = new THistManager("EGAhistos");
  std::array<std::string, 5> triggers = {"EG1", "EG2", "DG1", "DG2", "MB"};
  for(const auto &t : triggers){
    fHistos->CreateTH2(Form("hColRowG1%s", t.c_str()), Form("Col-Row distribution of online G1 patches for trigger %s", t.c_str()), 48, -0.5, 47.5, 104, -0.5, 103.5);
    fHistos->CreateTH2(Form("hColRowG2%s", t.c_str()), Form("Col-Row distribution of online G2 patches for trigger %s", t.c_str()), 48, -0.5, 47.5, 104, -0.5, 103.5);
  }

  PostData(1, fHistos->GetListOfHistograms());
}

bool AliAnalysisTaskEGAMonitor::IsEventSelected(){
  if(!fAliAnalysisUtils->IsVertexSelected2013pA(InputEvent())) return false;
  if(fAliAnalysisUtils->IsPileUpEvent(InputEvent())) return false;
  if(!(fInputHandler->IsEventSelected() & (AliVEvent::kEMCEGA | AliVEvent::kINT7))) return false;
  return true;
}

bool AliAnalysisTaskEGAMonitor::Run(){
  std::vector<std::string> triggers;
  if(fInputHandler->IsEventSelected() & AliVEvent::kEMCEGA){
    if(InputEvent()->GetFiredTriggerClasses().Contains("EG1")) triggers.push_back("EG1");
    if(InputEvent()->GetFiredTriggerClasses().Contains("EG2")) triggers.push_back("EG2");
    if(InputEvent()->GetFiredTriggerClasses().Contains("DG1")) triggers.push_back("DG1");
    if(InputEvent()->GetFiredTriggerClasses().Contains("DG2")) triggers.push_back("DG2");
  } else {
    triggers.push_back("MB");
  }

  if(fUseRecalcPatches){
    for(auto p : *(this->fTriggerPatchInfo)){
      AliEMCALTriggerPatchInfo *patch = static_cast<AliEMCALTriggerPatchInfo *>(p);
      if(!patch->IsGammaLowRecalc()) continue;
      if(patch->GetADCAmp() > fRecalcLow){
        for(const auto &t : triggers) fHistos->FillTH2(Form("hColRowG2%s", t.c_str()), patch->GetColStart(), patch->GetRowStart());
      }
      if(patch->GetADCAmp() > fRecalcHigh){
        for(const auto &t : triggers) fHistos->FillTH2(Form("hColRowG1%s", t.c_str()), patch->GetColStart(), patch->GetRowStart());
      }
    }
  } else {
    AliVCaloTrigger *emctrigraw = InputEvent()->GetCaloTrigger("EMCAL");

    emctrigraw->Reset();
    Int_t col(-1), row(-1), triggerbits(0);
    while(emctrigraw->Next()){
      emctrigraw->GetTriggerBits(triggerbits);
      if(!(triggerbits & (BIT(kL1GammaHigh) | BIT(kL1GammaLow)))) continue;

      emctrigraw->GetPosition(col, row);
      if(triggerbits & BIT(kL1GammaHigh)){
        for(const auto &t : triggers) fHistos->FillTH2(Form("hColRowG1%s", t.c_str()), col, row);
      }
      if(triggerbits & BIT(kL1GammaLow)){
        for(const auto &t : triggers) fHistos->FillTH2(Form("hColRowG2%s", t.c_str()), col, row);
      }
    }
  }

  return true;
}

} /* namespace EMCalTriggerPtAnalysis */