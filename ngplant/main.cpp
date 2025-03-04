/***************************************************************************

 Copyright (C) 2006  Sergey Prokhorchuk

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***************************************************************************/

#include <sys/stat.h>

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/cmdline.h>
#include <wx/dir.h>
#include <wx/stdpaths.h>

#if !defined(__WXMSW__) && !defined(__WXPM__)
 #include "images/ngplant.xpm"
#endif

#include <ngpcore/p3dmodel.h>
#include <ngpcore/p3dmodelstemtube.h>
#include <ngpcore/p3dmodelstemquad.h>
#include <ngpcore/p3dbalgbase.h>
#include <ngpcore/p3dbalgstd.h>
#include <ngpcore/p3diostream.h>
#include <ngpcore/p3dcompat.h>

#include <ngput/p3dospath.h>

#include <images/p3dnotexxpm.h>
#include <images/p3dclosexpm.h>

#include <p3dmedit.h>
#include <p3dnga.h>
#include <p3dmaterialstd.h>
#include <p3dcanvas3d.h>
#include <p3duiappopt.h>
#include <p3dwxcurvectrl.h>
#include <p3dpluglua.h>

#include <p3dlog.h>

#include <p3dapp.h>

#define NGPLANT_BASE_VER "0.9.14"

#if defined(EXTRA_VERSION)
 #define NGPLANT_VERSION_STRING NGPLANT_BASE_VER "(" EXTRA_VERSION ")"
#else
 #define NGPLANT_VERSION_STRING NGPLANT_BASE_VER
#endif

enum
 {
  wxID_EXPORT      = wxID_HIGHEST + 1200,
  wxID_EXPORT_NGA,
  wxID_IMPORT,
  wxID_IMPORT_NGA,
  wxID_RUN_SCRIPT,
  wxID_SHOW_DUMMIES,
  wxID_OPEN_RECENT,

  wxID_EXPORT_PLUGIN_FIRST = wxID_OPEN_RECENT + 10000,
  wxID_EXPORT_PLUGIN_LAST  = wxID_EXPORT_PLUGIN_FIRST + 250,

  wxID_RECENT_FILES_FIRST = wxID_EXPORT_PLUGIN_LAST + 1,
  wxID_RECENT_FILES_LAST  = wxID_RECENT_FILES_FIRST + P3D_RECENT_FILES_MAX - 1
 };

P3DApp *P3DApp::SelfPtr = NULL;

BEGIN_EVENT_TABLE(P3DMainFrame,wxFrame)
 EVT_MENU(wxID_NEW,P3DMainFrame::OnNew)
 EVT_MENU(wxID_OPEN,P3DMainFrame::OnOpen)
 EVT_MENU_RANGE(wxID_RECENT_FILES_FIRST,wxID_RECENT_FILES_LAST,P3DMainFrame::OnOpenRecent)
 EVT_MENU(wxID_SAVE,P3DMainFrame::OnSave)
 EVT_MENU(wxID_SAVEAS,P3DMainFrame::OnSaveAs)
 EVT_MENU(wxID_EXPORT_NGA,P3DMainFrame::OnExportNga)
 EVT_MENU_RANGE(wxID_EXPORT_PLUGIN_FIRST,wxID_EXPORT_PLUGIN_LAST,P3DMainFrame::OnExportObjPlugin)
 EVT_MENU(wxID_IMPORT_NGA,P3DMainFrame::OnImportNga)
 EVT_MENU(wxID_RUN_SCRIPT,P3DMainFrame::OnRunScript)
 EVT_MENU(wxID_SHOW_DUMMIES,P3DMainFrame::OnShowDummy)
 EVT_MENU(wxID_EXIT,P3DMainFrame::OnQuit)
 EVT_MENU(wxID_ABOUT,P3DMainFrame::OnAbout)
 EVT_MENU(wxID_PREFERENCES,P3DMainFrame::OnEditPreferences)
 EVT_MENU(wxID_UNDO,P3DMainFrame::OnUndo)
 EVT_MENU(wxID_REDO,P3DMainFrame::OnRedo)
 EVT_CLOSE(P3DMainFrame::OnFrameClose)
END_EVENT_TABLE()

class P3DUndoRedoMenuStateUpdater
 {
  public           :

                   P3DUndoRedoMenuStateUpdater
                                      (wxMenuBar          *MenuBar,
                                       const P3DEditCommandQueue
                                                          *CommandQueue)
   {
    Queue         = CommandQueue;
    this->MenuBar = MenuBar;

    UndoEmpty = Queue->UndoQueueEmpty();
    RedoEmpty = Queue->RedoQueueEmpty();
   }

                  ~P3DUndoRedoMenuStateUpdater
                                      ()
   {
    wxMenuItem                        *Item;

    if (UndoEmpty != Queue->UndoQueueEmpty())
     {
      Item = MenuBar->FindItem(wxID_UNDO);

      if (Item != NULL)
       {
        Item->Enable(UndoEmpty);
       }
     }

    if (RedoEmpty != Queue->RedoQueueEmpty())
     {
      Item = MenuBar->FindItem(wxID_REDO);

      if (Item != NULL)
       {
        Item->Enable(RedoEmpty);
       }
     }
   }

  private          :

  const P3DEditCommandQueue           *Queue;
  wxMenuBar                           *MenuBar;
  bool                                 UndoEmpty;
  bool                                 RedoEmpty;
 };

                   P3DMainFrame::P3DMainFrame  (const wxChar   *title)
                    : wxFrame(NULL,wxID_ANY,title,wxDefaultPosition,wxSize(640,400))
 {
  wxMenu          *FileMenu = new wxMenu();
  wxMenu          *EditMenu = new wxMenu();
  wxMenu          *ViewMenu = new wxMenu();
  wxMenu          *HelpMenu = new wxMenu();
  wxMenu          *ImportMenu = new wxMenu();
  wxMenu          *ExportMenu = new wxMenu();
  const P3DPluginInfoVector
                  &ExportPlugins = P3DApp::GetApp()->GetExportPlugins();
  int              MenuItemId = wxID_EXPORT_PLUGIN_FIRST;

  SetIcon(wxICON(ngplant));

  ExportMenu->Append(wxID_EXPORT_NGA,wxT("ngPlant archive .NGA"));

  for (unsigned int Index = 0; Index < ExportPlugins.size(); Index++)
   {
    ExportMenu->Append(MenuItemId++,wxString(ExportPlugins[Index].GetMenuName(),wxConvUTF8));
   }

  ExportMenu->Append(wxID_RUN_SCRIPT,wxT("Run export script..."));

  ImportMenu->Append(wxID_IMPORT_NGA,wxT("ngPlant archive .NGA"));

  P3DRecentFiles *RecentFiles = P3DApp::GetApp()->GetRecentFiles();

  wxMenu *RecentFilesMenu = RecentFiles->CreateMenu();

  FileMenu->Append(wxID_NEW,wxT("&New\tCtrl-N"));
  FileMenu->Append(wxID_OPEN,wxT("&Open...\tCtrl-O"));

  FileMenu->Append(wxID_OPEN_RECENT,wxT("Open Recent"),RecentFilesMenu);

  if (RecentFiles->IsEmpty())
   {
    FileMenu->Enable(wxID_OPEN_RECENT,false);
   }

  FileMenu->Append(wxID_SAVE,wxT("&Save\tCtrl-S"));
  FileMenu->Append(wxID_SAVEAS,wxT("Save as..."));
  FileMenu->Append(wxID_EXPORT,wxT("Export to"),ExportMenu);
  FileMenu->Append(wxID_IMPORT,wxT("Import from"),ImportMenu);
  FileMenu->Append(wxID_PREFERENCES,wxT("Preferences..."));
  FileMenu->Append(wxID_EXIT,wxT("E&xit\tAlt-X"));

  wxMenuItem *Item;

  Item = EditMenu->Append(wxID_UNDO,wxT("Undo\tCtrl-Z"));
  Item->Enable(false);

  Item = EditMenu->Append(wxID_REDO,wxT("&Redo\tCtrl-R"));
  Item->Enable(false);

  Item = ViewMenu->AppendCheckItem(wxID_SHOW_DUMMIES,wxT("Show &dummies\tCtrl-H"));
  Item->Check(P3DApp::GetApp()->IsDummyVisible());

  HelpMenu->Append(wxID_ABOUT,wxT("About..."));

  wxMenuBar       *MenuBar = new wxMenuBar();
  MenuBar->Append(FileMenu,wxT("&File"));
  MenuBar->Append(EditMenu,wxT("&Edit"));
  MenuBar->Append(ViewMenu,wxT("&View"));
  MenuBar->Append(HelpMenu,wxT("&Help"));

  Canvas3D = new P3DCanvas3D(this);

  P3DApp::GetApp()->GetTexManager()->SetCanvas(Canvas3D);

  EditPanel = new P3DModelEditPanel(this);

  wxBoxSizer *main_sizer = new wxBoxSizer(wxHORIZONTAL);

  main_sizer->Add(Canvas3D,1,wxEXPAND | wxALL,2);
  main_sizer->Add(EditPanel,0,wxEXPAND | wxALL,2);

  SetMenuBar(MenuBar);

  SetSizer(main_sizer);

  main_sizer->Fit(this);
  main_sizer->SetSizeHints(this);
 }

void               P3DMainFrame::OnQuit   (wxCommandEvent     &event)
 {
  if (ApproveDataLoss())
   {
    // We're about to generate an OnFrameClose event, and don't want to be asked again.
    // So this call simply sets UnsavedChanges to 'false' by using the fact
    // that setting model to the same value doesn't do anything except for
    // reseting UnsavedChanges value.
    P3DApp::GetApp()->SetModel(P3DApp::GetApp()->GetModel());

    Close();
   }
 }

void               P3DMainFrame::OnFrameClose   (wxCloseEvent     &event)
 {
  if (ApproveDataLoss())
   {
    Destroy();
   }
 }

void               P3DMainFrame::OnAbout  (wxCommandEvent     &event)
 {
  ::wxMessageBox(wxT("ngPlant ") wxT(NGPLANT_VERSION_STRING) wxT("\n\nCopyright (c) 2006-2014 Sergey Prokhorchuk\nProject page: ngplant.sourceforge.net\n\nReleased under the GNU General Public License"),
                 wxT("About - ngPlant"),
                 wxOK,
                 this);
 }

void               P3DMainFrame::OnSave   (wxCommandEvent     &event)
 {
  if (P3DApp::GetApp()->GetFileName().empty())
   {
    OnSaveAs(event);
   }
  else
   {
    P3DApp::GetApp()->SaveModel(P3DApp::GetApp()->GetFileName().mb_str());
   }
 }

void               P3DMainFrame::OnSaveAs (wxCommandEvent     &event)
 {
  wxString                                 FileNameStr;

  FileNameStr = ::wxFileSelector(wxT("File name"),wxT(""),wxT(""),wxT(".ngp"),wxT("*.ngp"),wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (!FileNameStr.empty())
   {
    wxFileName FileName(FileNameStr);

    if (!FileName.HasExt())
     {
      FileName.SetExt(wxT("ngp"));
     }

    P3DApp::GetApp()->SaveModel(FileName.GetFullPath().mb_str());
   }
 }

void               P3DMainFrame::OnEditPreferences
                                          (wxCommandEvent     &event)
 {
  P3DAppOptDialog  PreferencesDialog(NULL,wxID_ANY);

  wxConfigBase                        *Cfg;

  Cfg = wxConfigBase::Get();

  if (Cfg->HasGroup(wxT("/TexLocations")))
   {
    Cfg->SetPath(wxT("/TexLocations"));

    bool     Cont;
    wxString EntryName;
    wxString PathStr;
    long     Cookie;

    Cont = Cfg->GetFirstEntry(EntryName,Cookie);

    while (Cont)
     {
      if (Cfg->Read(EntryName,&PathStr))
       {
        PreferencesDialog.AddTexPath(PathStr.mb_str(wxConvUTF8));
       }

      Cont = Cfg->GetNextEntry(EntryName,Cookie);
     }

    Cfg->SetPath(wxT("/"));
   }

  unsigned char R,G,B;

  P3DApp::GetApp()->GetGroundColor(&R,&G,&B);

  PreferencesDialog.SetGroundColor(R,G,B);
  PreferencesDialog.SetGroundVisible(P3DApp::GetApp()->IsGroundVisible());

  P3DApp::GetApp()->GetBackgroundColor(&R,&G,&B);
  PreferencesDialog.SetBackgroundColor(R,G,B);

  PreferencesDialog.SetExport3DPrefs(P3DApp::GetApp()->GetExport3DPrefs());
  PreferencesDialog.SetCameraControlPrefs(*P3DApp::GetApp()->GetCameraControlPrefs());
  PreferencesDialog.SetRenderQuirksPrefs(P3DApp::GetApp()->GetRenderQuirksPrefs());
  PreferencesDialog.SetModelPrefs(P3DApp::GetApp()->GetModelPrefs());

  PreferencesDialog.SetPluginsPath(P3DApp::GetApp()->GetPluginsPath());

  PreferencesDialog.SetCurveCtrlPrefs
   (P3DCurveCtrl::BestWidth,P3DCurveCtrl::BestHeight);

  if (PreferencesDialog.ShowModal() == wxID_OK)
   {
    #if wxCHECK_VERSION(2,8,0)
     {
      Cfg->DeleteGroup(wxT("/TexLocations"));
     }
    #else /* It look's like a bug in wxWidgets 2.4.X (wxConfigFile) */
     {
      if (Cfg->HasGroup(wxT("/TexLocations")))
       {
        Cfg->SetPath(wxT("/TexLocations"));

        bool     Cont;
        wxString EntryName;
        wxString PrevName;
        long     Cookie;

        Cont = Cfg->GetFirstEntry(EntryName,Cookie);

        while (Cont)
         {
          PrevName = EntryName;

          Cont = Cfg->GetNextEntry(EntryName,Cookie);

          Cfg->DeleteEntry(PrevName,FALSE);
         }

        Cfg->SetPath(wxT("/"));
       }
     }
    #endif

    Cfg->SetPath(wxT("/TexLocations"));

    for (unsigned int Index = 0;
         Index < PreferencesDialog.GetTexPathsCount();
         Index++)
     {
      wxString EntryName = wxString::Format(wxT("Location%u"),Index);

      Cfg->Write(EntryName,wxString(PreferencesDialog.GetTexPath(Index),wxConvUTF8));
     }

    Cfg->SetPath(wxT("/"));

    P3D3DViewPrefs View3DPrefs;

    PreferencesDialog.GetGroundColor(&View3DPrefs.GroundColor.R,
                                     &View3DPrefs.GroundColor.G,
                                     &View3DPrefs.GroundColor.B);
    P3DApp::GetApp()->SetGroundColor(View3DPrefs.GroundColor.R,
                                     View3DPrefs.GroundColor.G,
                                     View3DPrefs.GroundColor.B);

    View3DPrefs.GroundVisible = PreferencesDialog.GetGroundVisible();

    P3DApp::GetApp()->SetGroundVisibility(View3DPrefs.GroundVisible);

    PreferencesDialog.GetBackgroundColor(&View3DPrefs.BackgroundColor.R,
                                         &View3DPrefs.BackgroundColor.G,
                                         &View3DPrefs.BackgroundColor.B);
    P3DApp::GetApp()->SetBackgroundColor(View3DPrefs.BackgroundColor.R,
                                         View3DPrefs.BackgroundColor.G,
                                         View3DPrefs.BackgroundColor.B);

    View3DPrefs.Save(Cfg);

    P3DApp::GetApp()->SetCameraControlPrefs(&PreferencesDialog.GetCameraControlPrefs());

    P3DApp::GetApp()->GetCameraControlPrefs()->Save(Cfg);

    PreferencesDialog.GetExport3DPrefs(P3DApp::GetApp()->GetExport3DPrefs());
    P3DApp::GetApp()->GetExport3DPrefs()->Save(Cfg);

    P3DApp::GetApp()->SetPluginsPath(PreferencesDialog.GetPluginsPath());

    Cfg->Write(wxT("/Paths/Plugins"),P3DApp::GetApp()->GetPluginsPath());

    unsigned int   NewBestWidth;
    unsigned int   NewBestHeight;

    PreferencesDialog.GetCurveCtrlPrefs(&NewBestWidth,&NewBestHeight);

    if ((NewBestWidth  != P3DCurveCtrl::BestWidth) ||
        (NewBestHeight != P3DCurveCtrl::BestHeight))
     {
      P3DCurveCtrl::BestWidth  = NewBestWidth;
      P3DCurveCtrl::BestHeight = NewBestHeight;

      EditPanel->UpdatePanelMinSize();
     }

    P3DUIControlsPrefs::Save(Cfg);

    P3DApp::GetApp()->SetRenderQuirksPrefs(PreferencesDialog.GetRenderQuirksPrefs());
    P3DApp::GetApp()->GetRenderQuirksPrefs().Save(Cfg);

    P3DApp::GetApp()->SetModelPrefs(PreferencesDialog.GetModelPrefs());
    P3DApp::GetApp()->GetModelPrefs().Save(Cfg);

    InvalidatePlant();
   }
 }

void               P3DMainFrame::OnExportNga
                                          (wxCommandEvent     &event)
 {
  P3DNGAExport();
 }

void               P3DMainFrame::OnExportObjPlugin
                                          (wxCommandEvent     &event)
 {
  int              PluginIndex;
  const P3DPluginInfoVector
                  &ExportPlugins = P3DApp::GetApp()->GetExportPlugins();

  PluginIndex = event.GetId() - wxID_EXPORT_PLUGIN_FIRST;

  if ((PluginIndex < 0) || (PluginIndex >= ExportPlugins.size()))
   {
    return;
   }

  P3DPlugLuaRunScript(ExportPlugins[PluginIndex].GetFileName(),
                      P3DApp::GetApp()->GetModel());
 }

void               P3DMainFrame::OnImportNga
                                          (wxCommandEvent     &event)
 {
  std::string NGPFileName = P3DNGAImport();

  if (!NGPFileName.empty())
   {
    if (wxMessageBox(wxString("Import succeeded. Do you want to open model ",wxConvUTF8) +
                     wxString(NGPFileName.c_str(),wxConvUTF8) + wxT(" now?"),
                     wxT("Information"),
                     wxYES_NO | wxICON_INFORMATION) == wxYES)
     {
      if (ApproveDataLoss())
       {
        OpenModelFile(wxString(NGPFileName.c_str(),wxConvUTF8));
       }
     }
   }
 }

void               P3DMainFrame::OnRunScript
                                          (wxCommandEvent     &event)
 {
  wxString                                 FileName;

  FileName = ::wxFileSelector(wxT("Select script to run"),wxT(""),wxT(""),wxT(".obj"),wxT("*.lua"),wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  if (!FileName.empty())
   {
    P3DPlugLuaRunScript(FileName.mb_str(),P3DApp::GetApp()->GetModel());
   }
 }

void               P3DMainFrame::OnNew(wxCommandEvent     &event)
 {
  if (ApproveDataLoss())
   {
    P3DPlantModel                     *NewModel;

    NewModel = P3DApp::GetApp()->CreateNewPlantModel();

    EditPanel->HideAll();

    P3DApp::GetApp()->SetModel(NewModel);

    P3DApp::GetApp()->SetFileName("");
    P3DApp::GetApp()->GetTexFS()->SetModelPath(0);

    EditPanel->RestoreAll();
   }
 }

bool               P3DMainFrame::ApproveDataLoss()
 {
  // If there's no unsaved work, there's nothing to worry about.
  if (!P3DApp::GetApp()->HasUnsavedChanges()) return true;

  // Otherwise ask the user if the loss of data is OK?
  if (::wxMessageBox(wxT("Your unsaved changes will be discarded. Are you sure?"),
                     wxT("Confirmation"),
                     wxOK | wxCANCEL) == wxOK)
   {
    return true;
   }
  else
   {
    return false;
   }
 }

void               P3DMainFrame::OnOpen
                                      (wxCommandEvent     &event)
 {
  wxString                             FileName;

  if (!ApproveDataLoss()) return;

  FileName = ::wxFileSelector(wxT("File name"),wxT(""),wxT(""),wxT(".ngp"),wxT("*.ngp"),wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  OpenModelFile(FileName);
 }

void               P3DMainFrame::OnOpenRecent
                                      (wxCommandEvent     &event)
 {
  if (!ApproveDataLoss()) return;

  int FileIndex = event.GetId() - wxID_RECENT_FILES_FIRST;

  const char *FileName = P3DApp::GetApp()->GetRecentFiles()->GetFileName(FileIndex);

  if (FileName != NULL)
   {
    OpenModelFile(wxString(FileName,wxConvUTF8));
   }
 }

bool               P3DMainFrame::OpenModelFile
                                      (const wxString     &FileName)
 {
  bool                                 Ok;
  P3DPlantModel                       *NewModel;

  Ok = false;

  if (!FileName.empty())
   {
    NewModel = 0;

    P3DIDEVFS  *TextureFS    = P3DApp::GetApp()->GetTexFS();
    std::string OldModelPath = TextureFS->GetModelPath() == 0 ?
                               std::string("") : std::string(TextureFS->GetModelPath());
    wxString    NewModelPath = wxFileName(FileName).GetPath();

    TextureFS->SetModelPath(NewModelPath.mb_str());

    try
     {
      P3DInputStringStreamFile         SourceStream;
      P3DIDEMaterialFactory            MaterialFactory
                                        (P3DApp::GetApp()->GetTexManager(),
                                         P3DApp::GetApp()->GetShaderManager());

      SourceStream.Open(FileName.mb_str());

      NewModel = new P3DPlantModel();

      NewModel->Load(&SourceStream,&MaterialFactory);

      SourceStream.Close();

      P3DApp::GetApp()->SetFileName(FileName.mb_str());
      P3DApp::GetApp()->GetRecentFiles()->OnFileOpened(FileName.mb_str());

      UpdateRecentFilesMenu();

      EditPanel->HideAll();

      P3DApp::GetApp()->SetModel(NewModel);

      NewModel = 0;

      EditPanel->RestoreAll();

      Ok = true;
     }
    catch (...)
     {
      if (OldModelPath.empty())
       {
        TextureFS->SetModelPath(0);
       }
      else
       {
        TextureFS->SetModelPath(OldModelPath.c_str());
       }

      ::wxMessageBox(wxT("Error while loading model"),wxT("Error"),wxOK | wxICON_ERROR);
     }

    delete NewModel;
   }

  return Ok;
 }

void               P3DMainFrame::OnUndo
                                      (wxCommandEvent     &event)
 {
  P3DApp::GetApp()->Undo();
 }

void               P3DMainFrame::OnRedo
                                      (wxCommandEvent     &event)
 {
  P3DApp::GetApp()->Redo();
 }

void               P3DMainFrame::OnShowDummy
                                      (wxCommandEvent     &event)
 {
  P3DApp::GetApp()->SetDummyVisible(event.IsChecked());
 }

void               P3DMainFrame::UpdateControls
                                      ()
 {
  EditPanel->UpdateControls();
 }

void               P3DMainFrame::Refresh3DView
                                      ()
 {
  Canvas3D->ForceRefresh();
 }

void               P3DMainFrame::InvalidatePlant
                                      ()
 {
  Refresh3DView();

  EditPanel->PlantInvalidated();
 }

void               P3DMainFrame::UpdateRecentFilesMenu
                                      ()
 {
  wxMenu     *FileMenu;
  wxMenuBar  *MenuBar             = GetMenuBar();
  wxMenuItem *RecentFilesMenuItem = MenuBar->FindItem(wxID_OPEN_RECENT,&FileMenu);

  P3DRecentFiles *RecentFiles = P3DApp::GetApp()->GetRecentFiles();

  if (RecentFilesMenuItem != NULL)
   {
    RecentFiles->UpdateMenu(RecentFilesMenuItem->GetSubMenu());

    FileMenu->Enable(wxID_OPEN_RECENT,!RecentFiles->IsEmpty());
   }
 }

IMPLEMENT_APP(P3DApp)

                   P3DApp::P3DApp     ()
 : MainFrame(0)
 {
 }

                   P3DApp::~P3DApp    ()
 {
  delete RecentFiles;
  delete CommandQueue;
  delete PlantModel;
 }

P3DPlantModel     *P3DApp::GetModel   ()
 {
  return(PlantModel);
 }

const P3DPlantObject
                  *P3DApp::GetPlantObject
                                      () const
 {
  if  ((PlantObjectDirty) && (PlantObjectAutoUpdate) &&
       (MainFrame->IsGLExtInited()))
   {
    bool InitMode;

    if (PlantObject == 0)
     {
      InitMode = true;
     }
    else
     {
      InitMode = false;

      delete PlantObject;
     }

    try
     {
      PlantObject      = new P3DPlantObject(PlantModel,RenderQuirks.UseColorArray);
      PlantObjectDirty = false;
     }
    catch (...)
     {
      PlantObject = 0;
     }

    if ((InitMode) && (PlantObject != 0))
     {
      MainFrame->EditPanel->PlantInvalidated();
     }
   }

  return(PlantObject);
 }

void               P3DApp::SetModel   (P3DPlantModel      *Model)
 {
  P3DUndoRedoMenuStateUpdater           Updater(MainFrame->GetMenuBar(),CommandQueue);

  CommandQueue->Clear();

  if (PlantModel != Model)
   {
    delete PlantModel;

    PlantModel = Model;

    InvalidatePlant();
   }

  UnsavedChanges = false;
 }

void               P3DApp::SaveModel  (const char         *FileName)
 {
  std::string OldModelPath = TexFS.GetModelPath() != 0 ?
                             std::string(TexFS.GetModelPath()) :
                             std::string("");

  TexFS.SetModelPath(wxFileName(wxString(FileName,wxConvUTF8)).GetPath().mb_str());

  try
   {
    P3DOutputStringStreamFile          TargetStream;
    P3DIDEMaterialSaver                MaterialSaver;

    TargetStream.Open(FileName);

    PlantModel->Save(&TargetStream,&MaterialSaver);

    TargetStream.Close();

    SetFileName(FileName);

    UnsavedChanges = false;
   }
  catch (...)
   {
    if (OldModelPath.empty())
     {
      TexFS.SetModelPath(0);
     }
    else
     {
      TexFS.SetModelPath(OldModelPath.c_str());
     }

    ::wxMessageBox(wxT("Error while saving model"),wxT("Error"),wxOK | wxICON_ERROR);
   }

  RecentFiles->OnFileSaved(FileName);
  MainFrame->UpdateRecentFilesMenu();
 }

P3DTexManagerGL   *P3DApp::GetTexManager
                                      ()
 {
  return(&TexManager);
 }

P3DShaderManager  *P3DApp::GetShaderManager
                                      ()
 {
  return(&ShaderManager);
 }

P3DIDEVFS         *P3DApp::GetTexFS   ()
 {
  return(&TexFS);
 }

P3DRecentFiles    *P3DApp::GetRecentFiles
                                      ()
 {
  return RecentFiles;
 }

const wxBitmap    &P3DApp::GetBitmap  (unsigned int        Bitmap)
 {
  return(Bitmaps[Bitmap]);
 }

P3DStemModelTube  *P3DApp::CreateStemModelTube
                                      () const
 {
  P3DStemModelTube *TubeStemModel = new P3DStemModelTube();

  TubeStemModel->SetLength(0.5f);

  P3DMathNaturalCubicSpline TempCurve;

  TempCurve.SetLinear(0.0f,1.0f,1.0f,0.0f);

  TubeStemModel->SetProfileScaleCurve(&TempCurve);

  return(TubeStemModel);
 }

P3DStemModelQuad  *P3DApp::CreateStemModelQuad
                                      () const
 {
  P3DStemModelQuad *QuadStemModel = new P3DStemModelQuad();

  QuadStemModel->SetLength(0.5f);
  QuadStemModel->SetWidth(0.5f);

  return(QuadStemModel);
 }

P3DBranchingAlg   *P3DApp::CreateBranchingAlgStd
                                      () const
 {
  return(new P3DBranchingAlgStd());
 }

P3DMaterialInstanceSimple
                  *P3DApp::CreateMatInstanceStd
                                      () const
 {
  P3DMaterialInstanceSimple           *Result;
  P3DMaterialDef                       StdMaterialDef;

  StdMaterialDef.SetColor(0.7f,0.7f,0.7f);

  Result = new P3DMaterialInstanceSimple(const_cast<P3DTexManagerGL*>(&TexManager),
                                         const_cast<P3DShaderManager*>(&ShaderManager),
                                         StdMaterialDef);

  return(Result);
 }

void               P3DApp::Refresh3DView
                                      ()
 {
  if (MainFrame != 0)
   {
    MainFrame->Refresh3DView();
   }
 }

void               P3DApp::InvalidatePlant
                                      ()
 {
  if (PlantObjectAutoUpdate)
   {
    ForceUpdate();
   }
  else
   {
    PlantObjectDirty = true;
    MainFrame->InvalidatePlant();
   }

  UnsavedChanges = true;
 }

void               P3DApp::InvalidateCamera
                                      ()
 {
  if (PlantObject != 0)
   {
    PlantObject->InvalidateCamera();
   }
 }

void               P3DApp::ForceUpdate()
 {
  if (MainFrame->IsGLExtInited())
   {
    delete PlantObject;

    try
     {
      PlantObject      = new P3DPlantObject(PlantModel,RenderQuirks.UseColorArray);
      PlantObjectDirty = false;
     }
    catch (...)
     {
      PlantObject = 0;
     }

    MainFrame->InvalidatePlant();
   }
 }

bool               P3DApp::IsPlantObjectDirty
                                      () const
 {
  return(PlantObjectDirty);
 }

bool               P3DApp::HasUnsavedChanges
                                      () const
 {
  return UnsavedChanges;
 }

wxString           P3DApp::GetFileName() const
 {
  return(PlantFileName);
 }

void               P3DApp::SetFileName(const char         *FileName)
 {
  PlantFileName = wxString(FileName,wxConvUTF8);

  if (PlantFileName.empty())
   {
    MainFrame->SetTitle(wxT("ngPlant designer"));
   }
  else
   {
    wxFileName       TempFileName(PlantFileName);

    wxString         ShortName;

    ShortName = TempFileName.GetFullName();

    MainFrame->SetTitle(wxString(wxT("ngPlant designer [")) + ShortName + wxT("]"));
   }
 }

wxString           P3DApp::GetDerivedFileName
                                      (const wxString     &FileNameExtension)
 {
  wxFileName DerivedFileName;

  if (PlantFileName.IsEmpty())
   {
    wxString BaseName = wxString(PlantModel->GetPlantBase()->GetName(),wxConvUTF8);

    DerivedFileName = wxFileName(BaseName);
   }
  else
   {
    DerivedFileName = PlantFileName;
   }

  DerivedFileName.SetExt(FileNameExtension);

  return DerivedFileName.GetFullName();
 }

void               P3DApp::InitTexFS  ()
 {
  wxConfigBase                        *Cfg;

  Cfg = wxConfigBase::Get();

  if (Cfg->HasGroup(wxT("/TexLocations")))
   {
    Cfg->SetPath(wxT("/TexLocations"));

    bool     Cont;
    wxString EntryName;
    wxString PathStr;
    long     Cookie;

    Cont = Cfg->GetFirstEntry(EntryName,Cookie);

    while (Cont)
     {
      if (Cfg->Read(EntryName,&PathStr))
       {
        TexFS.AppendEntry(PathStr.mb_str(wxConvUTF8));
       }

      Cont = Cfg->GetNextEntry(EntryName,Cookie);
     }

    Cfg->SetPath(wxT("/"));
   }
 }

P3DPlantModel     *P3DApp::CreateNewPlantModel
                                      () const
 {
  P3DPlantModel                       *NewPlantModel;
  P3DBranchModel                      *TrunkModel;
  P3DStemModelTube                    *TrunkStemModel;
  P3DBranchingAlgBase                 *BranchingAlg;

  NewPlantModel = new P3DPlantModel();

  NewPlantModel->SetBaseSeed(123);
  NewPlantModel->GetPlantBase()->SetName("Plant");

  TrunkModel     = new P3DBranchModel();
  TrunkStemModel = new P3DStemModelTube();
  BranchingAlg   = new P3DBranchingAlgBase();

  TrunkStemModel->SetLength(15.0f);
  TrunkStemModel->SetProfileResolution(ModelPrefs.TubeCrossSectResolution[0]);

  P3DMathNaturalCubicSpline TempCurve;

  TempCurve.SetLinear(0.0f,1.0f,1.0f,0.0f);

  TrunkStemModel->SetProfileScaleCurve(&TempCurve);

  TrunkModel->SetName("Branch-1");
  TrunkModel->SetStemModel(TrunkStemModel);
  TrunkModel->SetMaterialInstance(CreateMatInstanceStd());
  TrunkModel->SetBranchingAlg(BranchingAlg);

  NewPlantModel->GetPlantBase()->AppendSubBranch(TrunkModel);

  return(NewPlantModel);
 }

void               P3DApp::ExecEditCmd(P3DEditCommand     *Cmd)
 {
  P3DUndoRedoMenuStateUpdater          Updater(MainFrame->GetMenuBar(),CommandQueue);

  CommandQueue->PushAndExec(Cmd);
 }

void               P3DApp::Undo       ()
 {
  P3DUndoRedoMenuStateUpdater          Updater(MainFrame->GetMenuBar(),CommandQueue);

  CommandQueue->Undo();

  UpdateControls();
 }

void               P3DApp::Redo       ()
 {
  P3DUndoRedoMenuStateUpdater          Updater(MainFrame->GetMenuBar(),CommandQueue);

  CommandQueue->Redo();

  UpdateControls();
 }

void               P3DApp::UpdateControls
                                      ()
 {
  MainFrame->UpdateControls();
 }

void               P3DApp::OnInitCmdLine
                                      (wxCmdLineParser    &Parser)
 {
  Parser.AddSwitch(wxT("ns"),wxT("no-shaders"),wxT("disable shaders"));
  Parser.AddSwitch(wxT("es"),wxT("enable-stderr"),wxT("route log messages to stderr"));
  Parser.AddParam(wxT("model file"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);

  wxApp::OnInitCmdLine(Parser);
 }

bool               P3DApp::OnCmdLineParsed
                                      (wxCmdLineParser    &Parser)
 {
  if (wxApp::OnCmdLineParsed(Parser))
   {
    if (Parser.Found(wxT("ns")))
     {
      DisableShaders();
     }

    if (Parser.Found(wxT("verbose")))
     {
      P3DLogEnableVerbose();
     }

    if (Parser.Found(wxT("es")))
     {
      P3DLogEnableStdErr();
     }

    if (Parser.GetParamCount() >= 1)
     {
      PlantFileName = Parser.GetParam(0);
     }

    return(true);
   }
  else
   {
    return(false);
   }
 }

void               P3DApp::ScanPlugins()
 {
  wxDir            Dir(PluginsPath);
  wxString         FileName;
  P3DPluginInfo    PluginInfo;

  ExportPlugins.clear();
  GMeshPlugins.clear();

  if (!Dir.IsOpened())
   {
    return;
   }

  if (Dir.GetFirst(&FileName,wxEmptyString,wxDIR_FILES)) do
   {
    try
     {
      P3DInputStringStreamFile         SourceStream;
      wxFileName                       FullFileName(PluginsPath,FileName);

      SourceStream.Open(FullFileName.GetFullPath().mb_str());

      if (PluginInfo.LoadInfo(&SourceStream,FullFileName.GetFullPath().mb_str()))
       {
        if      (PluginInfo.GetType() == P3DPluginTypeModelExport)
         {
          ExportPlugins.push_back(PluginInfo);
         }
        else if (PluginInfo.GetType() == P3DPluginTypeGMeshGenerator)
         {
          GMeshPlugins.push_back(PluginInfo);
         }
       }

      SourceStream.Close();
     }
    catch (...)
     {
     }
   }
  while (Dir.GetNext(&FileName));
 }

bool               P3DApp::OnInit     ()
 {
  UseShaders = true;

  SelfPtr = this;
  UnsavedChanges = false;
  DummyVisible   = false;

  if (!wxApp::OnInit())
   {
    return(false);
   }

  ::wxInitAllImageHandlers();

  wxFileName                           CfgFileName;

  CfgFileName.AssignHomeDir();
  CfgFileName.AppendDir(wxT(".ngplant"));

  struct stat  CfgDirStat;

  if (stat(CfgFileName.GetFullPath().mb_str(wxConvUTF8),&CfgDirStat) != 0)
   {
    #ifdef _WIN32
    mkdir(CfgFileName.GetFullPath().mb_str(wxConvUTF8));
    #else
    mkdir(CfgFileName.GetFullPath().mb_str(wxConvUTF8),0775);
    #endif
   }

  CfgFileName.SetFullName(wxT(".ngplant"));

  wxConfigBase                        *Cfg;

  Cfg = new wxFileConfig(wxT("ngplant"),
                         wxEmptyString,
                         CfgFileName.GetFullPath(),
                         wxEmptyString,
                         wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_NO_ESCAPE_CHARACTERS);

  wxConfigBase::Set(Cfg);

  InitTexFS();

  View3DPrefs.Read(Cfg);
  CameraControlPrefs.Read(Cfg);
  P3DUIControlsPrefs::Read(Cfg);
  Export3DPrefs.Read(Cfg);
  RenderQuirks.Read(Cfg);
  ModelPrefs.Read(Cfg);

  if (!Cfg->Read(wxT("Paths/Plugins"),&PluginsPath))
   {
    #if defined(PLUGINS_DIR)
    PluginsPath = wxT(PLUGINS_DIR);
    #elif defined(__WXOSX__)
    wxFileName TmpPluginsPath = wxFileName(wxStandardPaths::Get().GetResourcesDir(),wxT(""));
    TmpPluginsPath.AppendDir(wxT("plugins"));

    PluginsPath = TmpPluginsPath.GetPath();
    #else
    PluginsPath = wxT("plugins");
    #endif
   }

  ScanPlugins();

  Bitmaps[P3D_BITMAP_NO_TEXTURE]     = wxBitmap(P3DNoTextureXpm);
  Bitmaps[P3D_BITMAP_REMOVE_TEXTURE] = wxBitmap(P3DCloseXPM);

  LODLevel = 1.0f;

  CommandQueue = new P3DEditCommandQueue();
  RecentFiles  = new P3DRecentFiles(wxID_RECENT_FILES_FIRST);

  PlantModel  = CreateNewPlantModel();
  PlantObject = 0;
  PlantObjectDirty = true;
  PlantObjectAutoUpdate = true;

  MainFrame = new P3DMainFrame(wxT("ngPlant designer"));

  MainFrame->Show(TRUE);

  if (!PlantFileName.empty())
   {
    if (!MainFrame->OpenModelFile(PlantFileName))
     {
      PlantFileName.Clear();
     }
   }

  ForceUpdate();

  return(TRUE);
 }

void               P3DApp::GetGroundColor
                                      (unsigned char      *R,
                                       unsigned char      *G,
                                       unsigned char      *B) const
 {
  *R = View3DPrefs.GroundColor.R;
  *G = View3DPrefs.GroundColor.G;
  *B = View3DPrefs.GroundColor.B;
 }

void               P3DApp::SetGroundColor
                                      (unsigned char       R,
                                       unsigned char       G,
                                       unsigned char       B)
 {
  View3DPrefs.GroundColor.R = R;
  View3DPrefs.GroundColor.G = G;
  View3DPrefs.GroundColor.B = B;

  MainFrame->InvalidatePlant();
 }

bool               P3DApp::IsGroundVisible
                                      () const
 {
  return(View3DPrefs.GroundVisible);
 }

void               P3DApp::SetGroundVisibility
                                      (bool                Visible)
 {
  if (View3DPrefs.GroundVisible != Visible)
   {
    View3DPrefs.GroundVisible = Visible;

    MainFrame->InvalidatePlant();
   }
 }

void               P3DApp::GetBackgroundColor
                                      (unsigned char      *R,
                                       unsigned char      *G,
                                       unsigned char      *B) const
 {
  *R = View3DPrefs.BackgroundColor.R;
  *G = View3DPrefs.BackgroundColor.G;
  *B = View3DPrefs.BackgroundColor.B;
 }

void               P3DApp::SetBackgroundColor
                                      (unsigned char       R,
                                       unsigned char       G,
                                       unsigned char       B)
 {
  View3DPrefs.BackgroundColor.R = R;
  View3DPrefs.BackgroundColor.G = G;
  View3DPrefs.BackgroundColor.B = B;

  MainFrame->InvalidatePlant();
 }

P3DExport3DPrefs  *P3DApp::GetExport3DPrefs
                                      ()
 {
  return(&Export3DPrefs);
 }

const P3DCameraControlPrefs
                  *P3DApp::GetCameraControlPrefs
                                      () const
 {
  return(&CameraControlPrefs);
 }

void               P3DApp::SetCameraControlPrefs
                                      (const P3DCameraControlPrefs
                                                          *Prefs)
 {
  this->CameraControlPrefs = *Prefs;
 }

void               P3DApp::SetPluginsPath
                                      (const wxString     &PluginsPath)
 {
  this->PluginsPath = PluginsPath;
 }

const wxString    &P3DApp::GetPluginsPath
                                      () const
 {
  return(PluginsPath);
 }

const P3DPluginInfoVector
                  &P3DApp::GetExportPlugins
                                      () const
 {
  return(ExportPlugins);
 }

const P3DPluginInfoVector
                  &P3DApp::GetGMeshPlugins
                                      () const
 {
  return(GMeshPlugins);
 }

float              P3DApp::GetLODLevel() const
 {
  return(LODLevel);
 }

void               P3DApp::SetLODLevel(float               Level)
 {
  LODLevel = P3DMath::Clampf(0.0f,1.0f,Level);
 }

bool               P3DApp::IsAutoUpdateMode
                                      () const
 {
  return(PlantObjectAutoUpdate);
 }

void               P3DApp::SetAutoUpdateMode
                                      (bool                Enable)
 {
  PlantObjectAutoUpdate = Enable;
 }

bool               P3DApp::IsDummyVisible
                                      () const
 {
  return DummyVisible;
 }

void               P3DApp::SetDummyVisible
                                      (bool                Visible)
 {
  if (DummyVisible != Visible)
   {
    DummyVisible = Visible;

    ForceUpdate();
   }
 }

bool               P3DApp::IsShadersEnabled
                                      () const
 {
  return(UseShaders);
 }

void               P3DApp::DisableShaders
                                      ()
 {
  UseShaders = false;

  ShaderManager.DisableShaders();
 }

const P3DRenderQuirksPrefs
                  &P3DApp::GetRenderQuirksPrefs
                                      () const
 {
  return(RenderQuirks);
 }

void               P3DApp::SetRenderQuirksPrefs
                                      (const P3DRenderQuirksPrefs
                                                          &Prefs)
 {
  RenderQuirks = Prefs;
 }

const P3DModelPrefs
                  &P3DApp::GetModelPrefs
                                      () const
 {
  return ModelPrefs;
 }

void             P3DApp::SetModelPrefs(const P3DModelPrefs&Prefs)
 {
  ModelPrefs = Prefs;
 }

P3DApp            *P3DApp::GetApp     ()
 {
  return SelfPtr;
 }

