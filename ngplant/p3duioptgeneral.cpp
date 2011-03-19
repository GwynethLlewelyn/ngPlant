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

#include <wx/wx.h>

#include <p3dapp.h>
#include <p3dcmdqueue.h>
#include <p3duioptgeneral.h>

enum
 {
  wxID_SEED_CTRL = wxID_HIGHEST + 1,
  wxID_LOD_CTRL,
  wxID_RANDOMNESS_STATE_CTRL
 };

BEGIN_EVENT_TABLE(P3DOptGeneralPanel,wxPanel)
 EVT_SPINSLIDER_VALUE_CHANGED(wxID_SEED_CTRL,P3DOptGeneralPanel::OnSeedChanged)
 EVT_SPINSLIDER_VALUE_CHANGED(wxID_LOD_CTRL,P3DOptGeneralPanel::OnLODChanged)
 EVT_CHECKBOX(wxID_RANDOMNESS_STATE_CTRL,P3DOptGeneralPanel::OnRandomnessStateChanged)
END_EVENT_TABLE()

                   P3DOptGeneralPanel::P3DOptGeneralPanel
                                      (wxWindow           *parent)
                   : P3DUIParamPanel(parent)
 {
  wxBoxSizer *TopSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticBoxSizer *SeedTopSizer  = new wxStaticBoxSizer(new wxStaticBox(this,wxID_STATIC,wxT("Randomness")),wxVERTICAL);
  wxFlexGridSizer  *SeedGridSizer = new wxFlexGridSizer(2,2,3,3);

  SeedGridSizer->AddGrowableCol(1);

  SeedGridSizer->Add(new wxStaticText(this,wxID_ANY,wxT("Seed")),0,wxALL | wxALIGN_CENTER_VERTICAL,1);

  wxSpinSliderCtrl *spin_slider = new wxSpinSliderCtrl(this,wxID_SEED_CTRL,wxSPINSLIDER_MODE_INTEGER,P3DApp::GetApp()->GetModel()->GetBaseSeed(),1,100000);
  spin_slider->SetStdStep(10);
  spin_slider->SetSmallStep(1);
  spin_slider->SetLargeMove(100);
  spin_slider->SetStdMove(10);
  spin_slider->SetSmallMove(1);

  SeedGridSizer->Add(spin_slider,1,wxALL | wxALIGN_RIGHT,1);

  SeedGridSizer->Add(new wxStaticText(this,wxID_ANY,wxT("Disable randomness")),0,wxALL | wxALIGN_CENTER_VERTICAL,1);

  wxCheckBox *RandomnessDisabledCheckBox = new wxCheckBox(this,wxID_RANDOMNESS_STATE_CTRL,wxT(""));
  RandomnessDisabledCheckBox->SetValue((P3DApp::GetApp()->GetModel()->GetFlags() & P3D_MODEL_FLAG_NO_RANDOMNESS) != 0);

  SeedGridSizer->Add(RandomnessDisabledCheckBox,1,wxALL | wxALIGN_RIGHT,1);

  SeedTopSizer->Add(SeedGridSizer,0,wxEXPAND,0);

  TopSizer->Add(SeedTopSizer,0,wxEXPAND | wxALL,1);

  wxStaticBoxSizer *LODTopSizer  = new wxStaticBoxSizer(new wxStaticBox(this,wxID_STATIC,wxT("LOD")),wxVERTICAL);
  wxFlexGridSizer  *LODGridSizer = new wxFlexGridSizer(1,2,3,3);

  LODGridSizer->AddGrowableCol(1);

  LODGridSizer->Add(new wxStaticText(this,wxID_ANY,wxT("LOD level")),0,wxALL | wxALIGN_CENTER_VERTICAL,1);

  spin_slider = new wxSpinSliderCtrl(this,wxID_LOD_CTRL,wxSPINSLIDER_MODE_FLOAT,P3DApp::GetApp()->GetLODLevel(),0.0f,1.0f);
  spin_slider->SetStdStep(0.1);
  spin_slider->SetSmallStep(0.01);
  spin_slider->SetLargeMove(0.05);
  spin_slider->SetStdMove(0.02);
  spin_slider->SetSmallMove(0.01);

  LODGridSizer->Add(spin_slider,1,wxALL | wxALIGN_RIGHT,1);

  LODTopSizer->Add(LODGridSizer,0,wxEXPAND,0);

  TopSizer->Add(LODTopSizer,0,wxEXPAND | wxALL,1);

  SetSizer(TopSizer);

  TopSizer->Fit(this);
  TopSizer->SetSizeHints(this);
 }

class P3DEditCmdChangeModelSeed : public P3DEditCommand
 {
  public           :

                   P3DEditCmdChangeModelSeed
                                      (unsigned int        NewSeed)
   {
    OldSeed       = P3DApp::GetApp()->GetModel()->GetBaseSeed();
    this->NewSeed = NewSeed;
   }

  virtual void     Exec               ()
   {
    P3DApp::GetApp()->GetModel()->SetBaseSeed(NewSeed);
    P3DApp::GetApp()->InvalidatePlant();
   }

  virtual void     Undo               ()
   {
    P3DApp::GetApp()->GetModel()->SetBaseSeed(OldSeed);
    P3DApp::GetApp()->InvalidatePlant();
   }

  private          :

  unsigned int     NewSeed;
  unsigned int     OldSeed;
 };

void               P3DOptGeneralPanel::OnSeedChanged
                                      (wxSpinSliderEvent  &event)
 {
  P3DApp::GetApp()->ExecEditCmd(new P3DEditCmdChangeModelSeed(event.GetIntValue()));
 }

void               P3DOptGeneralPanel::OnLODChanged
                                      (wxSpinSliderEvent  &event)
 {
  P3DApp::GetApp()->SetLODLevel(event.GetFloatValue());

  P3DApp::GetApp()->InvalidatePlant();
 }

void               P3DOptGeneralPanel::OnRandomnessStateChanged
                                      (wxCommandEvent     &event)
 {
  P3DPlantModel* Model = P3DApp::GetApp()->GetModel();
  unsigned int   Flags = Model->GetFlags();

  if (event.IsChecked())
   {
    Flags |= P3D_MODEL_FLAG_NO_RANDOMNESS;
   }
  else
   {
    Flags &= ~P3D_MODEL_FLAG_NO_RANDOMNESS;
   }

  Model->SetFlags(Flags);
  P3DApp::GetApp()->InvalidatePlant();
 }

void               P3DOptGeneralPanel::UpdateControls
                                      ()
 {
  wxSpinSliderCtrl *SpinSlider;

  SpinSlider = (wxSpinSliderCtrl*)FindWindow(wxID_SEED_CTRL);

  if (SpinSlider != NULL)
   {
    SpinSlider->SetValue(P3DApp::GetApp()->GetModel()->GetBaseSeed());
   }

  wxCheckBox *CheckBox;

  CheckBox = (wxCheckBox*)FindWindow(wxID_RANDOMNESS_STATE_CTRL);

  if (CheckBox != NULL)
   {
    CheckBox->SetValue
     ((P3DApp::GetApp()->GetModel()->GetFlags() & P3D_MODEL_FLAG_NO_RANDOMNESS) != 0);
   }
 }

