// Usage:
//   makeResults.C("tasks", "file_list", ""task_id, kGRID)
//   tasks : "ALL" or one/more of the following separated by space:
//     "EFF"  : TRD Tracking Efficiency 
//     "EFFC" : TRD Tracking Efficiency Combined (barrel + stand alone) - only in case of simulations
//     "RES"  : TRD tracking Resolution
//     "PID"  : TRD PID - pion efficiency 
//     "DET"  : Basic TRD Detector checks
//     "NOFR" : Data set does not have AliESDfriends.root 
//     "NOMC" : Data set does not have Monte Carlo Informations (real data), so all tasks which rely
//              on MC information are switched off
//   file_list : is the list of the files to be processed. 
//     They should contain the full path. Here is an example:
// /lustre/alice/local/TRDdata/SIM/P-Flat/TRUNK/RUN0/TRD.Performance.root
// or for GRID alien:///alice/cern.ch/user/m/mfasel/MinBiasProd/results/ppMinBias80000/1/TRD.Performance.root
//   task_id : identifier of task speciality as defined by the AddMacro.C. 
//             (e.g. AddTRDresolution.C defines "" for barrel tracks, "K" for kink tracks and "SA" for stand alone tracks)
//   kGRID : specify if files are comming from a GRID collection [default kFALSE]
//
// HOW TO BUILD THE FILE LIST
//   1. locally
// ls -1 BaseDir/RUN*/TRD.Performance.root > files.lst
// 
//   2. on Grid
// char *BaseDir="/alice/cern.ch/user/m/mfasel/MinBiasProd/results/ppMinBias80000/";
// TGrid::Connect("alien://");
// TGridResult *res = gGrid->Query(BaseDir, "%/TRD.Performance.root");
// TGridCollection *col = gGrid->OpenCollectionQuery(res);
// col->Reset();
// TMap *map = 0x0;
// while(map = (TMap*)col->Next()){
//   TIter it((TCollection*)map);
//   TObjString *info = 0x0;
//   while(info=(TObjString*)it()){
//     printf("alien://%s\n", col->GetLFN(info->GetString().Data()));
//   }
// }
//
// The files which will be processed are the intersection between the
// condition on the tasks and the files in the file list.
//
//
// In compiled mode :
// Don't forget to load first the libraries
// gSystem->Load("libANALYSIS")
// gSystem->Load("libANALYSISalice")
// gSystem->Load("libTender");
// gSystem->Load("libPWGPP");
// gSystem->Load("libCORRFW");
// gSystem->Load("libPWGmuon");
// gSystem->Load("libNetx") ;
// gSystem->Load("libRAliEn");

// Authors:
//   Alex Bercuci (A.Bercuci@gsi.de)
//   Markus Fasel (m.Fasel@gsi.de)
//

#if ! defined (__CINT__) || defined (__MAKECINT__)
#include <fstream>
#include "TError.h"
#include <TClass.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TGraph.h>
#include <TObjArray.h>
#include <TObjString.h>
#include <TString.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TGrid.h>
#include <TGridResult.h>
#include <TGridCollection.h>

#include <AliLog.h>

#include <AliTRDtrendingManager.h>
#include <AliTRDpwgppHelper.h>
#include <AliTRDrecoTask.h>
#include <AliTRDcheckESD.h>
#include <AliTRDinfoGen.h>
#endif

const Char_t *libs[] = {"libANALYSIS", "libCORRFW", "libTender", "libPWGPP"};
// define setup
TCanvas *c(NULL);
Bool_t mc(kFALSE), friends(kFALSE);
Bool_t summary(kTRUE);

void processTRD(TNamed* task, const Char_t *filename);
void processESD(TNamed* task, const Char_t *filename);
void processGEN(TNamed* task, const Char_t *filename);
void makeSummaryESD(const Char_t* filename="QAresults.root", 
		    Double_t* trendValues=0x0, 
		    Bool_t useCF=kTRUE, 
		    Bool_t useIsolatedBC=kFALSE, 
		    Bool_t cutTOFbc=kFALSE, 
		    const Char_t* dir="TRD_Performance", 
		    Bool_t isGrid=kFALSE);
void makeResults(const Char_t *opt = "ALL",
                 const Char_t *files="QAresults.root",
                 const Char_t *cid = "",
                 Bool_t kGRID=kFALSE, Bool_t dosummary = kTRUE)
{
  if(kGRID) TGrid::Connect("alien://");

  // Load Libraries in interactive mode
  Int_t nlibs = static_cast<Int_t>(sizeof(libs)/sizeof(Char_t *));
  for(Int_t ilib=0; ilib<nlibs; ilib++){
    if(gSystem->Load(libs[ilib]) >= 0) continue;
    Error("makeResults.C", "Failed to load %s.", libs[ilib]);
    return;
  }

  mc      = AliTRDpwgppHelper::HasReadMCData(opt);
  friends = AliTRDpwgppHelper::HasReadFriendData(opt);

  gStyle->SetOptStat(0);
  gStyle->SetOptFit(0);
  TString outputFile;
  if(!TString(files).EndsWith(".root")){
    outputFile = Form("%s/QAResults.root", gSystem->ExpandPathName("$PWD"));
    AliTRDpwgppHelper::MergeProd("QAResults.root", files);
  } else {
    outputFile = files;
  }
  Int_t fSteerTask = AliTRDpwgppHelper::ParseOptions(opt);

  if(!dosummary){
    summary = kFALSE;
    if(!c) c=new TCanvas("c", "Performance", 10, 10, 800, 500);
  }

  TClass *ctask = new TClass;
  AliAnalysisTask *task = NULL;
  for(Int_t itask = AliTRDpwgppHelper::kNTRDQATASKS; itask--;){
    if(!AliTRDpwgppHelper::DoTask(itask, fSteerTask)) continue;
    new(ctask) TClass(AliTRDpwgppHelper::TaskClassName(itask));
    task = (AliAnalysisTask*)ctask->New();
    task->SetName(Form("%s%s", task->GetName(), cid));
    printf(" *** task %s, input QA file \"%s\"\n", task->GetName(), outputFile.Data());
    if(task->IsA()->InheritsFrom("AliTRDrecoTask")) processTRD(task, outputFile.Data());
    else if(strcmp(task->IsA()->GetName(), "AliTRDcheckESD")==0) processESD(task, outputFile.Data());
    else if(strcmp(task->IsA()->GetName(), "AliTRDinfoGen")==0) processGEN(task, outputFile.Data());
    else{
      Error("makeResults.C", "Handling of class task \"%s\" not implemented.", task->IsA()->GetName());
      delete task;
    }
  }
  delete ctask;
  delete c;
  // write trending summary
  AliTRDtrendingManager::Instance()->Terminate();
}


//______________________________________________________
void processTRD(TNamed *otask, const Char_t *filename)
{
  printf("process[%s] : %s\n", otask->GetName(), otask->GetTitle());
  Int_t debug(0);
  AliTRDrecoTask *task = dynamic_cast<AliTRDrecoTask*>(otask);
  task->SetDebugLevel(debug);
  AliLog::SetClassDebugLevel(otask->IsA()->GetName(), debug);
  task->SetMCdata(mc);
  task->SetFriends(friends);

  //if(!task->Load(Form("%s/AnalysisResults.root", gSystem->ExpandPathName("$PWD")))){
  if(!task->Load(filename)){
    Error("makeResults.C", "Load data container for task %s failed.", task->GetName());
    delete task;
    return;
  }
  task->LoadDetectorMap(filename);
  if(!task->PostProcess()){
    Error("makeResults.C", "Processing data container for task %s failed.", task->GetName());
    delete task;
    return;
  }
  if(summary) task->MakeSummary();
  else{
    for(Int_t ipic=0; ipic<task->GetNRefFigures(); ipic++){
      c->Clear();
      if(!task->GetRefFigure(ipic)) continue;
      c->SaveAs(Form("%s_Fig%02d.gif", task->GetName(), ipic), "gif");
    }
  }
  delete task;
}

//______________________________________________________
void processESD(TNamed *otask, const Char_t *filename)
{
  printf("process[%s] : %s\n", otask->GetName(), otask->GetTitle());

  AliTRDcheckESD *esd = dynamic_cast<AliTRDcheckESD*>(otask);
  if(!esd){
    Info("makeResults.C", "Processing of task %s failed.", otask->GetName());
    delete otask;
    return;
  }
  //if(!esd->Load(Form("%s/AnalysisResults.root", gSystem->ExpandPathName("$PWD")), "TRD_Performance")){
  if(!esd->Load(filename, "TRD_Performance")){
    Error("makeResults.C", "Load data container for task %s failed.", esd->GetName());
    delete esd;
    return;
  }
  esd->Terminate(NULL);

  if(summary) esd->MakeSummaryFromCF(0, "", kFALSE, kFALSE);
  // else{
  //   for(Int_t ipic(0); ipic<esd->GetNRefFigures(); ipic++){
  //     c->Clear();
  //     if(!esd->GetRefFigure(ipic)) continue;
  //     c->SaveAs(Form("%s_Fig%02d.gif", esd->GetName(), ipic));
  //   }
  // }
  delete esd;
}

//______________________________________________________
void processGEN(TNamed *otask, const Char_t *filename)
{
  printf("process[%s] : %s\n", otask->GetName(), otask->GetTitle());

  AliTRDinfoGen *info = dynamic_cast<AliTRDinfoGen*>(otask);

  if(!info->Load(filename, "TRD_Performance")){
    Error("makeResults.C", "Load data container for task %s failed.", info->GetName());
    delete info;
    return;
  }
  info->MakeSummary();
//   else{
//     for(Int_t ipic(0); ipic<info->GetNRefFigures(); ipic++){
//       c->Clear();
//       if(!info->GetRefFigure(ipic)) continue;
//       c->SaveAs(Form("%s_Fig%02d.gif", info->GetName(), ipic));
//     }
//   }

  delete info;
}

//______________________________________________________
void makeSummaryESD(const Char_t* filename, Double_t* trendValues, Bool_t useCF, Bool_t useIsolatedBC, Bool_t cutTOFbc, const Char_t* dir, Bool_t isGrid) {
  //
  //  Make the summary picture and get trending variables from the ESD task
  //
  if(isGrid) TGrid::Connect("alien://");

  // Load Libraries in interactive mode
  Int_t nlibs = static_cast<Int_t>(sizeof(libs)/sizeof(Char_t *));
  for(Int_t ilib=0; ilib<nlibs; ilib++){
    if(gSystem->Load(libs[ilib]) >= 0) continue;
    Error("makeResults.C", "Failed to load %s.", libs[ilib]);
    return;
  }

  AliTRDcheckESD *esd=new AliTRDcheckESD();
  if(!esd->Load(filename,dir)) return;
  //esd->Terminate();
  if(useCF) esd->MakeSummaryFromCF(trendValues, "", useIsolatedBC, cutTOFbc);
  //  else esd->MakeSummary(trendValues);
}
