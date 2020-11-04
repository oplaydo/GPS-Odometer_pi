//
// This file is part of GPS Odometer, a plugin for OpenCPN.
// based on the original version of dashboard.
//

/* $Id: odometer_pi.cpp, v1.0 2010/08/05 SethDart Exp $
 *
 * Project:  OpenCPN
 * Purpose:  Dashboard Plugin
 * Author:   Jean-Eudes Onfray
 *
 */

 /**************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 */

// wxWidgets Precompiled Headers
#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif 

#include <wx/msgdlg.h>  // Message box for test purposes (wxMessageBox)

#include "odometer_pi.h"

#include "version.h"

#include <typeinfo>
#include "icons.h"

// Global variables for fonts
wxFont *g_pFontTitle;
wxFont *g_pFontData;
wxFont *g_pFontLabel;
wxFont *g_pFontSmall;


// Preferences, Units and Values

int       g_iShowSpeed = 1;
int       g_iShowDepArrTimes = 1;
int       g_iShowTripLeg = 1;
int       g_iOdoSpeedMax;
int       g_iOdoOnRoute;
int       g_iOdoUTCOffset;
int       g_iOdoSpeedUnit;
int       g_iOdoDistanceUnit;
int       g_iResetTrip = 0; 
int       g_iResetLeg = 0;


// Watchdog timer, performs two functions, firstly refresh the odometer every second,  
// and secondly, if no data is received, set instruments to zero (eg. Engine switched off)
// BUG BUG Zeroing instruments not yet implemented
wxDateTime watchDogTime;

#if !defined(NAN)
static const long long lNaN = 0xfff8000000000000;
#define NAN (*(double*)&lNaN)
#endif

#ifdef __OCPN__ANDROID__
#include "qdebug.h"
#endif

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
    return (opencpn_plugin *) new odometer_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
    delete p;
}

#ifdef __OCPN__ANDROID__

QString qtStyleSheet = "QScrollBar:horizontal {\
border: 0px solid grey;\
background-color: rgb(240, 240, 240);\
height: 35px;\
margin: 0px 1px 0 1px;\
}\
QScrollBar::handle:horizontal {\
background-color: rgb(200, 200, 200);\
min-width: 20px;\
border-radius: 10px;\
}\
QScrollBar::add-line:horizontal {\
border: 0px solid grey;\
background: #32CC99;\
width: 0px;\
subcontrol-position: right;\
subcontrol-origin: margin;\
}\
QScrollBar::sub-line:horizontal {\
border: 0px solid grey;\
background: #32CC99;\
width: 0px;\
subcontrol-position: left;\
subcontrol-origin: margin;\
}\
QScrollBar:vertical {\
border: 0px solid grey;\
background-color: rgb(240, 240, 240);\
width: 35px;\
margin: 1px 0px 1px 0px;\
}\
QScrollBar::handle:vertical {\
background-color: rgb(200, 200, 200);\
min-height: 20px;\
border-radius: 10px;\
}\
QScrollBar::add-line:vertical {\
border: 0px solid grey;\
background: #32CC99;\
height: 0px;\
subcontrol-position: top;\
subcontrol-origin: margin;\
}\
QScrollBar::sub-line:vertical {\
border: 0px solid grey;\
background: #32CC99;\
height: 0px;\
subcontrol-position: bottom;\
subcontrol-origin: margin;\
}\
QCheckBox {\
spacing: 25px;\
}\
QCheckBox::indicator {\
width: 30px;\
height: 30px;\
}\
";

#endif

#ifdef __OCPN__ANDROID__
#include <QtWidgets/QScroller>
#endif


//---------------------------------------------------------------------------------------------------------
//
//    Odometer PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

// !!! WARNING !!!
// do not change the order, add new instruments at the end, before ID_DBP_LAST_ENTRY!
// otherwise, for users with an existing opencpn configuration file, their instruments are changing !
enum { ID_DBP_D_SOG, ID_DBP_I_SUMLOG, ID_DBP_I_TRIPLOG, ID_DBP_I_DEPART, ID_DBP_I_ARRIV,
       ID_DBP_B_TRIPRES, ID_DBP_I_LEGDIST, ID_DBP_I_LEGTIME, ID_DBP_B_LEGRES,
       ID_DBP_LAST_ENTRY /* this has a reference in one of the routines; defining a "LAST_ENTRY" and
       setting the reference to it, is one codeline less to change (and find) when adding new
       instruments :-)  */
};

// Retrieve a caption for each instrument
wxString GetInstrumentCaption(unsigned int id) {
    switch(id) {
        case ID_DBP_D_SOG:
            return _("Speedometer");
        case ID_DBP_I_SUMLOG:
            return _("Sum Log Distance");
        case ID_DBP_I_TRIPLOG:
            return _("Trip Log Distance");
        case ID_DBP_B_TRIPRES:
            return _("Reset Trip");
        case ID_DBP_I_DEPART:
            return _("Departure & Arrival");
        case ID_DBP_I_ARRIV:
            return _("");
        case ID_DBP_I_LEGDIST:
            return _("Leg Distance & Time");
        case ID_DBP_I_LEGTIME:
            return _("");
        case ID_DBP_B_LEGRES:
            return _("Reset Leg");
		default:
			return _T("");
    }
}

// Populate an index, caption and image for each instrument for use in a list control
void GetListItemForInstrument(wxListItem &item, unsigned int id) {
    item.SetData(id);
    item.SetText(GetInstrumentCaption(id));
   
	switch(id) {
        case ID_DBP_D_SOG:
			item.SetImage(1);
			break;
        case ID_DBP_I_SUMLOG:
        case ID_DBP_I_TRIPLOG:
        case ID_DBP_B_TRIPRES:
        case ID_DBP_I_DEPART:
        case ID_DBP_I_ARRIV:
        case ID_DBP_I_LEGDIST:
        case ID_DBP_I_LEGTIME:
        case ID_DBP_B_LEGRES:
			item.SetImage(0);
			break;
    }
}


// Constructs an id for the odometer instance
wxString MakeName() {
    return _T("ODOMETER");
}

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

odometer_pi::odometer_pi(void *ppimgr) : opencpn_plugin_116(ppimgr), wxTimer(this) {
    // Create the PlugIn icons
    initialize_images();
}

// Odometer Destructor
odometer_pi::~odometer_pi(void) {
      delete _img_odometer_colour;
}

// Initialize the Odometer
int odometer_pi::Init(void) {
    AddLocaleCatalog(_T("opencpn-odometer_pi"));

    // Used at startup, once started the plugin only uses version 2 configuration style
    m_config_version = -1;
    
    // Load the fonts
    g_pFontTitle = new wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);
    g_pFontData = new wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    g_pFontLabel = new wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    g_pFontSmall = new wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

    // Wire up the OnClose AUI event
    m_pauimgr = GetFrameAuiManager();
    m_pauimgr->Connect(wxEVT_AUI_PANE_CLOSE, wxAuiManagerEventHandler(odometer_pi::OnPaneClose), NULL, this);

    // Get a pointer to the opencpn configuration object
    m_pconfig = GetOCPNConfigObject();

    // And load the configuration items
    LoadConfig();

    // Scaleable Vector Graphics (SVG) icons are stored in the following path.
//	wxString shareLocn = GetPluginDataDir("odometer_pi") +  _T("/data/");

    wxString iconFolder = GetPluginDataDir("gps-odometer_pi") + wxFileName::GetPathSeparator() + _T("data") + wxFileName::GetPathSeparator();

    wxString normalIcon = iconFolder + _T("gps-odometer.svg");
    wxString toggledIcon = iconFolder + _T("gps-odometer_toggled.svg");
    wxString rolloverIcon = iconFolder + _T("gps-odometer_rollover.svg");
 
    // For journeyman styles, we prefer the built-in raster icons which match the rest of the toolbar.
/*
    if (GetActiveStyleName().Lower() != _T("traditional")) {
	normalIcon = iconFolder + _T("odometer.svg");
	toggledIcon = iconFolder + _T("odometer_toggled.svg");
	rolloverIcon = iconFolder + _T("odometer_rollover.svg");
    }   */

    // Add toolbar icon (in SVG format)
    m_toolbar_item_id = InsertPlugInToolSVG(_T(""), normalIcon, rolloverIcon, toggledIcon, wxITEM_CHECK,
	    _("GPS Odometer"), _T(""), NULL, ODOMETER_TOOL_POSITION, 0, this);

   
    // Having Loaded the config, then display each of the odometer
    ApplyConfig();

    // If we loaded a version 1 configuration, convert now to version 2, 
    if(m_config_version == 1) {
        SaveConfig();
    }

    // Initialize the watchdog timer
    Start(1000, wxTIMER_CONTINUOUS);

    // Reduced from the original odometer requests
    return (WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL | WANTS_PREFERENCES | WANTS_CONFIG | WANTS_NMEA_SENTENCES | USES_AUI_MANAGER);
}

bool odometer_pi::DeInit(void) {
    // Save the current configuration
    SaveConfig();

    // Is watchdog timer started?
    if (IsRunning()) {
	Stop(); 
    }

    // This appears to close each odometer instance
    OdometerWindow *odometer_window = m_ArrayOfOdometerWindow.Item(0)->m_pOdometerWindow;
    if (odometer_window) {
        m_pauimgr->DetachPane(odometer_window);
        odometer_window->Close();
        odometer_window->Destroy();
        m_ArrayOfOdometerWindow.Item(0)->m_pOdometerWindow = NULL;
    }

    // And this appears to close each odometer container
    OdometerWindowContainer *pdwc = m_ArrayOfOdometerWindow.Item(0);
    delete pdwc;

    // Unload the fonts
    delete g_pFontTitle;
    delete g_pFontData;
    delete g_pFontLabel;
    delete g_pFontSmall;
    return true;
}

// Called for each timer tick, refreshes each display
void odometer_pi::Notify()
{
    // Force a repaint of each instrument panel
    OdometerWindow *odometer_window = m_ArrayOfOdometerWindow.Item(0)->m_pOdometerWindow;
	if (odometer_window) {
	    odometer_window->Refresh();
	}

    //  Manage the watchdogs, watch messages used
    mGGA_Watchdog--;
    if( mGGA_Watchdog <= 0 ) {
        GPSQuality = 0;
        mGGA_Watchdog = gps_watchdog_timeout_ticks;
    }

    mGSV_Watchdog--;
    if( mGSV_Watchdog <= 0 ) {
        mSatsInView = m_NMEA0183.Gsv.SatsInView;
        mGSV_Watchdog = gps_watchdog_timeout_ticks;
    }

    mRMC_Watchdog--;
    if( mRMC_Watchdog <= 0 ) {
        SendSentenceToAllInstruments( OCPN_DBP_STC_SOG, NAN, _T("-") );
        mRMC_Watchdog = gps_watchdog_timeout_ticks;
    }
}

int odometer_pi::GetAPIVersionMajor() {
    return OCPN_API_VERSION_MAJOR;
}

int odometer_pi::GetAPIVersionMinor() {
    return OCPN_API_VERSION_MINOR;
}

int odometer_pi::GetPlugInVersionMajor() {
    return PLUGIN_VERSION_MAJOR;
}

int odometer_pi::GetPlugInVersionMinor() {
    return PLUGIN_VERSION_MINOR;
}

wxString odometer_pi::GetCommonName() {
    return _T(PLUGIN_COMMON_NAME);
}

wxString odometer_pi::GetShortDescription() {
    return _(PLUGIN_SHORT_DESCRIPTION);
//    return _T(PLUGIN_SHORT_DESCRIPTION);
}

wxString odometer_pi::GetLongDescription() {
    return _(PLUGIN_LONG_DESCRIPTION);
//    return _T(PLUGIN_LONG_DESCRIPTION);
}

// The plugin bitmap is loaded by the call to InitializeImages in icons.cpp
// Use png2wx.pl perl script to generate the binary data used in icons.cpp
wxBitmap *odometer_pi::GetPlugInBitmap() {
    return _img_odometer_colour; 
}

// Sends the data value from the parsed NMEA sentence to each gauge
void odometer_pi::SendSentenceToAllInstruments(int st, double value, wxString unit) {
    OdometerWindow *odometer_window = m_ArrayOfOdometerWindow.Item(0)->m_pOdometerWindow;
	if (odometer_window) {
	    odometer_window->SendSentenceToAllInstruments(st, value, unit);
	}
}

// This method is invoked by OpenCPN when we specify WANTS_NMEA_SENTENCES
void odometer_pi::SetNMEASentence(wxString &sentence) 
{

    m_NMEA0183 << sentence;

    if (m_NMEA0183.PreParse()) {
        if( m_NMEA0183.LastSentenceIDReceived == _T("GGA") ) {
            if( m_NMEA0183.Parse() ) {
                GPSQuality = m_NMEA0183.Gga.GPSQuality;
                mGGA_Watchdog = gps_watchdog_timeout_ticks;
            }
        }

        // GSV is currently not used.
        else if( m_NMEA0183.LastSentenceIDReceived == _T("GSV") ) {
            if( m_NMEA0183.Parse() ) {
                mSatsInView = m_NMEA0183.Gsv.SatsInView;
                mGSV_Watchdog = gps_watchdog_timeout_ticks;
            }
        }
        
        else if (m_NMEA0183.LastSentenceIDReceived == _T("RMC")) {
            if (m_NMEA0183.Parse() ) {
                if( m_NMEA0183.Rmc.IsDataValid == NTrue ) {

                    /* If GGA is missing or invalid you may get random values. 
                       Minimize the risk for error.  */

                    if ((GPSQuality < 5) && (GPSQuality > 0 )) {

                        // With SOG filter function
                        SendSentenceToAllInstruments( OCPN_DBP_STC_SOG,
                            toUsrSpeed_Plugin( mSOGFilter.filter(m_NMEA0183.Rmc.SpeedOverGroundKnots),
                                g_iOdoSpeedUnit ), getUsrSpeedUnit_Plugin( g_iOdoSpeedUnit ) );

                        // Need filterad speed as variable
                        CurrSpeed = toUsrSpeed_Plugin( mSOGFilter.filter(m_NMEA0183.Rmc.SpeedOverGroundKnots) );

                        // Date and time are wxStrings, instruments use double
                        dt = m_NMEA0183.Rmc.Date + m_NMEA0183.Rmc.UTCTime;
                        mUTCDateTime.ParseFormat( dt.c_str(), _T("%d%m%y%H%M%S") );
                        mRMC_Watchdog = gps_watchdog_timeout_ticks;
                    }
                }
            }
        } 
    }

    /* If GGA is missing or invalid you may get random values. Minimize the risk for error. */

    if ((GPSQuality >= 5) || (GPSQuality <= 0 )) {
        GPSQuality = 0; 
        CurrSpeed = 0.0;
    }
    Odometer(); 
}

void odometer_pi::Odometer() {

    //  Adjust time to local time zone used by departure and arrival times
    UTCTime = wxDateTime::Now();
    double offset = ((g_iOdoUTCOffset-24)*30); 
    wxTimeSpan TimeOffset(0, offset,0);
    LocalTime = UTCTime.Add(TimeOffset);

    // First time start
    if (m_DepTime == "2020-01-01 00:00:00") {
        m_DepTime = LocalTime.Format(wxT("%F %T"));
        m_ArrTime = LocalTime.Format(wxT("%F %T"));
    }

    /* TODO: There must be a better way to receive the reset event from 'OdometerInstrument_Button' 
             but using a global variable for transfer.  */
    if (g_iResetTrip == 1) {                             
        SetDepTime = 1;
        UseSavedDepTime = 0;
        UseSavedArrTime = 0;
        UseSavedTrip = 0;
        DepTimeShow = 0;
        m_DepTime = "---"; 
        m_ArrTime = "---";
        TripDist = 0.0;
        m_TripDist << TripDist;
        // SaveConfig();              // BUG: Does not save config file
        g_iResetTrip = 0;
    } 

    if (g_iResetLeg == 1) {  
        LegDist = 0.0; 
        m_LegDist << LegDist;
        m_LegTime = "---";
        LegStart = LocalTime; 
        // SaveConfig();              // BUG: Does not save config file
        g_iResetLeg = 0;
    } 

    // Set departure time to local time if CurrSpeed >  OnRouteSpeed
    m_OnRouteSpeed = g_iOdoOnRoute;
    // Reset after arrival, before system shutdown
    if ((CurrSpeed > m_OnRouteSpeed) && m_DepTime == "---" )  { 
        m_DepTime = LocalTime.Format(wxT("%F %T"));
    }

    // Reset after power up, before trip start
    if ((CurrSpeed > m_OnRouteSpeed) && SetDepTime == 1 )  {   
        m_DepTime = LocalTime.Format(wxT("%F %T"));
        SetDepTime = 0;
    }

    // Select departure time to use and enable if sppeed is enough
    if (CurrSpeed > m_OnRouteSpeed && DepTimeShow == 0 )  {
        if (UseSavedDepTime == 0) {
            DepTime = LocalTime; 
        } else {
            DepTime.ParseDateTime(m_DepTime); 
        }
        DepTimeShow = 1;
        strDep = DepTime.Format(wxT("%F %R"));
    } else {
        if (DepTimeShow == 0) strDep = " --- ";
        if (UseSavedDepTime == 1) strDep = m_DepTime.Truncate(16);  // Cut seconds
    }
    SendSentenceToAllInstruments(OCPN_DBP_STC_DEPART, ' ' , strDep );

    // Set and display arrival time 
    if (DepTimeShow == 1 )  {
        if (CurrSpeed > m_OnRouteSpeed) {
            strArr = _("On Route");
            ArrTimeShow = 0;
            UseSavedArrTime = 0;
        } else {
            if (ArrTimeShow == 0 ) { 
                m_ArrTime = LocalTime.Format(wxT("%F %T")); 
                ArrTime = LocalTime;
                ArrTimeShow = 1;
                strArr = ArrTime.Format(wxT("%F %R")); 
            }
        }
    } else {
        strArr = " --- ";  
    } 
    if (UseSavedArrTime == 1 ) strArr = m_ArrTime.Truncate(16);  // Cut seconds
    SendSentenceToAllInstruments(OCPN_DBP_STC_ARRIV, ' ' , strArr );

    // Distances
    if (UseSavedTrip == 1) {
        TotDist = 0.0;
        m_TotDist.ToDouble( &TotDist );
        TripDist = 0.0;
        m_TripDist.ToDouble( &TripDist );
        UseSavedTrip = 0;
    }

    if (UseSavedLeg == 1) {
        LegDist = 0.0;
        m_LegDist.ToDouble( &LegDist );

        LegStart = LocalTime;
        wxString strhrs = m_LegTime.Mid(0,2);
        double hrs = wxAtoi(strhrs);
        wxString strmins = m_LegTime.Mid(3,2);
        double mins = wxAtoi(strmins);
        wxString strsecs = m_LegTime.Mid(6,2);
        double secs = wxAtoi(strsecs);

        wxTimeSpan LegTime(hrs,mins,secs,0);
        LegStart = LegStart.Subtract(LegTime);
        UseSavedLeg = 0;
    }

    GetDistance();

    // Need not save full double or spaces
    TotDist = (TotDist + StepDist); 
    m_TotDist = " ";
    m_TotDist.Printf("%.1f",TotDist);
    m_TotDist.Trim(0);
    m_TotDist.Trim(1);

    TripDist = (TripDist + StepDist);
    m_TripDist = " ";
    m_TripDist.Printf("%.1f",TripDist);
    m_TripDist.Trim(0);
    m_TripDist.Trim(1);

    LegDist = (LegDist + StepDist);
    if (g_iShowTripLeg != 1) LegDist = 0.0;  // avoid overcount
    m_LegDist = " ";
    m_LegDist.Printf("%.2f",LegDist);
    m_LegDist.Trim(0);
    m_LegDist.Trim(1);

    SendSentenceToAllInstruments(OCPN_DBP_STC_SUMLOG, TotDist , DistUnit );
    SendSentenceToAllInstruments(OCPN_DBP_STC_TRIPLOG, TripDist , DistUnit );
    SendSentenceToAllInstruments(OCPN_DBP_STC_LEGDIST, LegDist , DistUnit );

    if (g_iShowTripLeg != 1) LegStart = LocalTime;  // avoid overcount
    LegTime = LocalTime.Subtract(LegStart); 
    m_LegTime = LegTime.Format("%H:%M:%S");

    wxString strLegTime;
    strLegTime = LegTime.Format("%H:%M:%S"); 
    SendSentenceToAllInstruments(OCPN_DBP_STC_LEGTIME, ' ' , strLegTime );  
}

void odometer_pi::GetDistance() {

    switch (g_iOdoDistanceUnit) {
        case 0:
            DistDiv = 3600;
            DistUnit = "M";
            break;
        case 1:
            DistDiv = 3128;
            DistUnit = "miles";
            break;
        case 2:
            DistDiv = 1944;
            DistUnit = "km";
            break;
    }

    CurrSec = wxAtoi(LocalTime.Format(wxT("%S")));

    // Calculate distance travelled during the elapsed time

    StepDist = 0.0;
    if (CurrSec != PrevSec) { 
        if (CurrSec > PrevSec) { 
            SecDiff = (CurrSec - PrevSec);
        } else {  
            PrevSec = (PrevSec - 58);  // Is this always ok no matter GPS update rates?
        }
        StepDist = (SecDiff * (CurrSpeed/DistDiv));

        /* TODO: Are at start randomly getting extreme values for distance (GPS position 
                 setting?) no matter if GPSQuality is ok. 
                 This delay seems to cure it (is this always one tick/second)?   */
        while ((StepCount < 15) && ( GPSQuality > 0)) {
            StepDist = 0.0;
            StepCount++ ;
        } 

        // No distance accepted while GPSQuality is invalid (equals 0)
        if (GPSQuality == 0) {
            StepDist = 0.0;
        } 
    }
    PrevSec = CurrSec;
}


// Not sure what this does, I guess we only install one toolbar item?? It is however required.
int odometer_pi::GetToolbarToolCount(void) {
    return 1;
}

//---------------------------------------------------------------------------------------------------------
//
// Odometer Setings Dialog
//
//---------------------------------------------------------------------------------------------------------

void odometer_pi::ShowPreferencesDialog(wxWindow* parent) {
	OdometerPreferencesDialog *dialog = new OdometerPreferencesDialog(parent, wxID_ANY, m_ArrayOfOdometerWindow);

    dialog->RecalculateSize();

#ifdef __OCPN__ANDROID__
    dialog->GetHandle()->setStyleSheet( qtStyleSheet);
#endif
    
#ifdef __OCPN__ANDROID__
    wxWindow *ccwin = GetOCPNCanvasWindow();

    if( ccwin ){
        int xmax = ccwin->GetSize().GetWidth();
        int ymax = ccwin->GetParent()->GetSize().GetHeight();  // This would be the Frame itself
        dialog->SetSize( xmax, ymax );
        dialog->Layout();
        
        dialog->Move(0,0);
    }
#endif

	if (dialog->ShowModal() == wxID_OK) {
		// Reload the fonts in case they have been changed
		delete g_pFontTitle;
		delete g_pFontData;
		delete g_pFontLabel;
		delete g_pFontSmall;

		g_pFontTitle = new wxFont(dialog->m_pFontPickerTitle->GetSelectedFont());
		g_pFontData = new wxFont(dialog->m_pFontPickerData->GetSelectedFont());
		g_pFontLabel = new wxFont(dialog->m_pFontPickerLabel->GetSelectedFont());
		g_pFontSmall = new wxFont(dialog->m_pFontPickerSmall->GetSelectedFont());

        /* Instrument visibility is not detected by ApplyConfig as no instuments are added,
           reordered or deleted. Globals are not checked at all by ApplyConfig.  */

        bool showSpeedDial = dialog->m_pCheckBoxShowSpeed->GetValue();
        bool showDepArrTimes = dialog->m_pCheckBoxShowDepArrTimes->GetValue();
        bool showTripLeg = dialog->m_pCheckBoxShowTripLeg->GetValue();

        if (showSpeedDial == true) {
            g_iShowSpeed = 1;
        } else {
            g_iShowSpeed = 0;
        }

        if (showDepArrTimes == true) {
            g_iShowDepArrTimes = 1;
        } else {
            g_iShowDepArrTimes = 0;
        }
 
        if (showTripLeg == true) {
            g_iShowTripLeg = 1;
        } else {
            g_iShowTripLeg = 0;
        }

        // Reload instruments and select panel
        OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
        cont->m_pOdometerWindow->SetInstrumentList(cont->m_aInstrumentList);
        OdometerWindow *d_w = cont->m_pOdometerWindow;
        wxAuiPaneInfo &pane = m_pauimgr->GetPane(d_w);

        // Update panel size
        wxSize sz = cont->m_pOdometerWindow->GetMinSize(); 

        /* TODO: These sizes are forced as dialog size and instruments messes up totally
                 otherwise, probably due to the use of checkboxes instead of general selection.
                 It is not perfect and should eventually be fixed somehow.
                 The height does not always compute properly. Sometimes need to restart plugin 
                 or OpenCPN to resize. Button width = 150, then add dialog frame = 10 incl slight
                 margin. 
                 This must be reworked!  */  

        sz.Set(160,125);  // Minimum size with Total distance, Trip distance and Trip reset.
        if (g_iShowSpeed == 1) sz.IncBy(0,170);       // Add for Speed instrument
        if (g_iShowDepArrTimes == 1) sz.IncBy(0,50);  // Add for departure/arrival times
        if (g_iShowTripLeg == 1) sz.IncBy(0,85);      // Add for trip dist, time and reset

        pane.MinSize(sz).BestSize(sz).FloatingSize(sz);
//        m_pauimgr->Update();

		// OnClose should handle that for us normally but it doesn't seems to do so
		// We must save changes first
		dialog->SaveOdometerConfig();
		m_ArrayOfOdometerWindow.Clear();
		m_ArrayOfOdometerWindow = dialog->m_Config;

		ApplyConfig();
		SaveConfig();   // TODO: Does not save configuration file

		// Not exactly sure what this does. Pesumably if no odometers are displayed, the toolbar icon 
        // is toggled/untoggled??
		SetToolbarItemState(m_toolbar_item_id, GetOdometerWindowShownCount() != 0);
	}

	// Invoke the dialog destructor
	dialog->Destroy();
}


void odometer_pi::SetColorScheme(PI_ColorScheme cs) {
    OdometerWindow *odometer_window = m_ArrayOfOdometerWindow.Item(0)->m_pOdometerWindow;
    if (odometer_window) {
		odometer_window->SetColorScheme(cs);
	}
}

int odometer_pi::GetToolbarItemId() { 
	return m_toolbar_item_id; 
}

int odometer_pi::GetOdometerWindowShownCount() {
    int cnt = 0;

    OdometerWindow *odometer_window = m_ArrayOfOdometerWindow.Item(0)->m_pOdometerWindow;
    if (odometer_window) {
        wxAuiPaneInfo &pane = m_pauimgr->GetPane(odometer_window);
        if (pane.IsOk() && pane.IsShown()) {
			cnt++;
		} 
    }
    return cnt;
}

void odometer_pi::OnPaneClose(wxAuiManagerEvent& event) {
    // if name is unique, we should use it
    OdometerWindow *odometer_window = (OdometerWindow *) event.pane->window;
    int cnt = 0;
    OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
    OdometerWindow *d_w = cont->m_pOdometerWindow;
    if (d_w) {
        // we must not count this one because it is being closed
        if (odometer_window != d_w) {
            wxAuiPaneInfo &pane = m_pauimgr->GetPane(d_w);
            if (pane.IsOk() && pane.IsShown()) {
				cnt++;
			}
        } else {
            cont->m_bIsVisible = false;
        }
    }
    SetToolbarItemState(m_toolbar_item_id, cnt != 0);

    event.Skip();
}

void odometer_pi::OnToolbarToolCallback(int id) {
    int cnt = GetOdometerWindowShownCount();
    bool b_anyviz = false;   // ???
    for (size_t i = 0; i < m_ArrayOfOdometerWindow.GetCount(); i++) {
        OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(i);
        if (cont->m_bIsVisible) {
            b_anyviz = true;
            break;   // This must be handled before removing the for statement
        }
    }

    OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
    OdometerWindow *odometer_window = cont->m_pOdometerWindow;
    if (odometer_window) {
        wxAuiPaneInfo &pane = m_pauimgr->GetPane(odometer_window);
        if (pane.IsOk()) {
            bool b_reset_pos = false;

#ifdef __WXMSW__
            //  Support MultiMonitor setups which an allow negative window positions.
            //  If the requested window title bar does not intersect any installed monitor,
            //  then default to simple primary monitor positioning.
            RECT frame_title_rect;
            frame_title_rect.left = pane.floating_pos.x;
            frame_title_rect.top = pane.floating_pos.y;
            frame_title_rect.right = pane.floating_pos.x + pane.floating_size.x;
            frame_title_rect.bottom = pane.floating_pos.y + 30;

			if (NULL == MonitorFromRect(&frame_title_rect, MONITOR_DEFAULTTONULL)) {
				b_reset_pos = true;
			}
#else

            //    Make sure drag bar (title bar) of window intersects wxClient Area of screen, with a
            //    little slop...
            wxRect window_title_rect;// conservative estimate
            window_title_rect.x = pane.floating_pos.x;
            window_title_rect.y = pane.floating_pos.y;
            window_title_rect.width = pane.floating_size.x;
            window_title_rect.height = 30;

            wxRect ClientRect = wxGetClientDisplayRect();
            ClientRect.Deflate(60, 60);// Prevent the new window from being too close to the edge
 			if (!ClientRect.Intersects(window_title_rect)) {
				b_reset_pos = true;
			}

#endif

			if (b_reset_pos) {
				pane.FloatingPosition(50, 50);
			}

            if (cnt == 0)
                if (b_anyviz)
                    pane.Show(cont->m_bIsVisible);
                else {
                   cont->m_bIsVisible = cont->m_bPersVisible;
                   pane.Show(cont->m_bIsVisible);
                }
            else
                pane.Show(false);
        }

        //  This patch fixes a bug in wxAUIManager
        //  FS#548
        // Dropping a Odometer Window right on top on the (supposedly fixed) chart bar window
        // causes a resize of the chart bar, and the Odometer window assumes some of its properties
        // The Odometer window is no longer grabbable...
        // Workaround:  detect this case, and force the pane to be on a different Row.
        // so that the display is corrected by toggling the odometer off and back on.
        if ((pane.dock_direction == wxAUI_DOCK_BOTTOM) && pane.IsDocked()) pane.Row(2);
    }
    // Toggle is handled by the toolbar but we must keep plugin manager b_toggle updated
    // to actual status to ensure right status upon toolbar rebuild
    SetToolbarItemState(m_toolbar_item_id, GetOdometerWindowShownCount() != 0);
    m_pauimgr->Update();
}

void odometer_pi::UpdateAuiStatus(void) {
    // This method is called after the PlugIn is initialized
    // and the frame has done its initial layout, possibly from a saved wxAuiManager "Perspective"
    // It is a chance for the PlugIn to syncronize itself internally with the state of any Panes that
    //  were added to the frame in the PlugIn ctor.

    OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
    wxAuiPaneInfo &pane = m_pauimgr->GetPane(cont->m_pOdometerWindow);
    // Initialize visible state as perspective is loaded now
    cont->m_bIsVisible = (pane.IsOk() && pane.IsShown()); 
    m_pauimgr->Update();
    
    // We use this callback here to keep the context menu selection in sync with the window state
    SetToolbarItemState(m_toolbar_item_id, GetOdometerWindowShownCount() != 0);
}

// Loads a saved configuration
bool odometer_pi::LoadConfig(void) {

    wxFileConfig *pConf = (wxFileConfig *) m_pconfig;

    if (pConf) {
        pConf->SetPath(_T("/PlugIns/GPS-Odometer"));

        wxString version;
        pConf->Read(_T("Version"), &version, wxEmptyString);
		wxString config;

        // Set some sensible defaults
        wxString TitleFont;
        wxString DataFont;
        wxString LabelFont;
        wxString SmallFont;
        
#ifdef __OCPN__ANDROID__
        TitleFont = _T("Roboto,16,-1,5,50,0,0,0,0,0");
        DataFont =  _T("Roboto,16,-1,5,50,0,0,0,0,0");
        LabelFont = _T("Roboto,16,-1,5,50,0,0,0,0,0");
        SmallFont = _T("Roboto,14,-1,5,50,0,0,0,0,0");
#endif        
        
        pConf->Read(_T("FontTitle"), &config, wxEmptyString);
		LoadFont(&g_pFontTitle, config);

		pConf->Read(_T("FontData"), &config, wxEmptyString);
        LoadFont(&g_pFontData, config);
        
		pConf->Read(_T("FontLabel"), &config, wxEmptyString);
		LoadFont(&g_pFontLabel, config);
		
        pConf->Read(_T("FontSmall"), &config, wxEmptyString);
		LoadFont(&g_pFontSmall, config);
		
		// Load the dedicated odometer settings plus set default values
        pConf->Read( _T("TotalDistance"), &m_TotDist, "0.0");  
        pConf->Read( _T("TripDistance"), &m_TripDist, "0.0");
        pConf->Read( _T("LegDistance"), &m_LegDist, "0.0");
        pConf->Read( _T("DepartureTime"), &m_DepTime, "2020-01-01 00:00:00");
        pConf->Read( _T("ArrivalTime"), &m_ArrTime, "2020-01-01 00:00:00");
        pConf->Read( _T("LegTime"), &m_LegTime, "00:00:00");

        pConf->Read(_T("SpeedometerMax"), &g_iOdoSpeedMax, 12);
        pConf->Read(_T("OnRouteSpeedLimit"), &g_iOdoOnRoute, 2);
        pConf->Read(_T("UTCOffset"), &g_iOdoUTCOffset, 24 );
        pConf->Read(_T("SpeedUnit"), &g_iOdoSpeedUnit, SPEED_KNOTS);
        pConf->Read(_T("DistanceUnit"), &g_iOdoDistanceUnit, DISTANCE_NAUTICAL_MILES);

		// Now retrieve the number of odometer containers and their instruments
        int d_cnt;
        pConf->Read(_T("OdometerCount"), &d_cnt, -1);
      
        // TODO: Memory leak? We should destroy everything first
        m_ArrayOfOdometerWindow.Clear();
        if (version.IsEmpty() && d_cnt == -1) {

        /* TODO Version 1 never genarated in OpenCPN 5.0 or later, section shall be removed
            m_config_version = 1;
            // Let's load version 1 or default settings.
            int i_cnt;
            pConf->Read(_T("InstrumentCount"), &i_cnt, -1);
            wxArrayInt ar;
            if (i_cnt != -1) {
                for (int i = 0; i < i_cnt; i++) {
                    int id;
                    pConf->Read(wxString::Format(_T("Instrument%d"), i + 1), &id, -1);
                    if (id != -1) ar.Add(id);
                }
            } else {  
            */
                // Load the default instrument list, do not change this order!
                ar.Add( ID_DBP_D_SOG );
                ar.Add( ID_DBP_I_SUMLOG );
                ar.Add( ID_DBP_I_TRIPLOG );
                ar.Add( ID_DBP_I_DEPART ); 
                ar.Add( ID_DBP_I_ARRIV ); 
                ar.Add( ID_DBP_B_TRIPRES );
                ar.Add( ID_DBP_I_LEGDIST );
                ar.Add( ID_DBP_I_LEGTIME );
                ar.Add( ID_DBP_B_LEGRES ); 
            // }
	    
	        // Note generate a unique GUID for each odometer container
            OdometerWindowContainer *cont = new OdometerWindowContainer(NULL, MakeName(), _("GPS Odometer"), _T("V"), ar);
            m_ArrayOfOdometerWindow.Add(cont);
            cont->m_bPersVisible = true;

        } else {
            // Configuration Version 2
            m_config_version = 2;
            bool b_onePersisted = false;

            wxString name;
            pConf->Read(_T("Name"), &name, MakeName());
            wxString caption;
            pConf->Read(_T("Caption"), &caption, _("Odometer"));
            wxString orient = "V";
//            int i_cnt;
//            pConf->Read(_T("InstrumentCount"), &i_cnt, -1);
            bool b_persist;
            pConf->Read(_T("Persistence"), &b_persist, 0);
            bool b_speedo;
            pConf->Read( _T("ShowSpeedometer"), &b_speedo, 1) ;
            bool b_deparr;
            pConf->Read( _T("ShowDepArrTimes"), &b_deparr, 1);
            bool b_tripleg;
            pConf->Read( _T("ShowTripLeg"), &b_tripleg, 1);

            // Allways 9 numerically ordered instruments in the array
            wxArrayInt ar;
            for (int i = 0; i < 9; i++) {
                ar.Add(i);

//                int id;
                /* Do not read from config, the order is fixed
                pConf->Read(wxString::Format(_T("Instrument%d"), i + 1), &id, -1); 
                if (id != -1) ar.Add(id);   */
            } 

			// TODO: Do not add if GetCount == 0

            OdometerWindowContainer *cont = new OdometerWindowContainer(NULL, name, caption, orient, ar);

            cont->m_bPersVisible = b_persist;

            cont->m_bShowSpeed = b_speedo;
            cont->m_bShowDepArrTimes = b_deparr;
            cont->m_bShowTripLeg = b_tripleg;

            // TODO: Using globals to pass these variables, works but is bad coding
            g_iShowSpeed = b_speedo;
            g_iShowDepArrTimes = b_deparr;
            g_iShowTripLeg = b_tripleg;

    		if (b_persist) {
	    	    b_onePersisted = true;
    		}
                
            m_ArrayOfOdometerWindow.Add(cont);

            
            // Make sure at least one odometer is scheduled to be visible
            if (m_ArrayOfOdometerWindow.Count() && !b_onePersisted){
                OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
                if (cont) {
	        	    cont->m_bPersVisible = true;
	        	}
            }   
        }
        return true;
    } else
        return false;
}

void odometer_pi::LoadFont(wxFont **target, wxString native_info)
{
    if( !native_info.IsEmpty() ){
#ifdef __OCPN__ANDROID__
        wxFont *nf = new wxFont( native_info );
        *target = nf;
#else
        (*target)->SetNativeFontInfo( native_info );
#endif
    }
}

bool odometer_pi::SaveConfig(void) {

    /* TODO: Does not save when called from 'odometer_pi::ShowPreferencesDialog' (or several 
             other routines), but works correct when starting/stopping OpenCPN.  */

    wxFileConfig *pConf = (wxFileConfig *) m_pconfig;

    if (pConf) {
        pConf->SetPath(_T("/PlugIns/GPS-Odometer"));
        pConf->Write(_T("Version"), _T("2"));
        pConf->Write(_T("FontTitle"), g_pFontTitle->GetNativeFontInfoDesc());
        pConf->Write(_T("FontData"), g_pFontData->GetNativeFontInfoDesc());
        pConf->Write(_T("FontLabel"), g_pFontLabel->GetNativeFontInfoDesc());
        pConf->Write(_T("FontSmall"), g_pFontSmall->GetNativeFontInfoDesc());

        pConf->Write( _T("TotalDistance"), m_TotDist);
        pConf->Write( _T("TripDistance"), m_TripDist);
        pConf->Write( _T("LegDistance"), m_LegDist);
        pConf->Write( _T("DepartureTime"), m_DepTime);
        pConf->Write( _T("ArrivalTime"), m_ArrTime);
        pConf->Write( _T("LegTime"), m_LegTime);

        pConf->Write(_T("SpeedometerMax"), g_iOdoSpeedMax);
        pConf->Write(_T("OnRouteSpeedLimit"), g_iOdoOnRoute);
        pConf->Write(_T("UTCOffset"), g_iOdoUTCOffset);
        pConf->Write(_T("SpeedUnit"), g_iOdoSpeedUnit);
        pConf->Write(_T("DistanceUnit"), g_iOdoDistanceUnit);

        pConf->Write(_T("OdometerCount"), (int) m_ArrayOfOdometerWindow.GetCount());
        OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
        pConf->Write(_T("Name"), cont->m_sName);
        pConf->Write(_T("Caption"), cont->m_sCaption);
        pConf->Write(_T("Persistence"), cont->m_bPersVisible);
        pConf->Write(_T("ShowSpeedometer"), cont->m_bShowSpeed);
        pConf->Write(_T("ShowDepArrTimes"), cont->m_bShowDepArrTimes);
        pConf->Write(_T("ShowTripLeg"), cont->m_bShowTripLeg);
/*
        pConf->Write(_T("InstrumentCount"), (int) cont->m_aInstrumentList.GetCount());
	    for (unsigned int j = 0; j < cont->m_aInstrumentList.GetCount(); j++) {
    		pConf->Write(wxString::Format(_T("Instrument%d"), j + 1), cont->m_aInstrumentList.Item(j));
	    }
*/

    return true;
	} else {
		return false;
	}
}

// Load current odometer containers and their instruments
// Called at start and when preferences dialogue closes
void odometer_pi::ApplyConfig(void) {

    // Reverse order to handle deletes
    for (size_t i = m_ArrayOfOdometerWindow.GetCount(); i > 0; i--) {
        OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(i - 1);
        int orient = 0 ;   // Always vertical ('0')
        if(!cont->m_pOdometerWindow) {  
            // A new odometer is created
            cont->m_pOdometerWindow = new OdometerWindow(GetOCPNCanvasWindow(), wxID_ANY,
                    m_pauimgr, this, orient, cont);
            cont->m_pOdometerWindow->SetInstrumentList(cont->m_aInstrumentList);
            bool vertical = orient == wxVERTICAL;
            wxSize sz = cont->m_pOdometerWindow->GetMinSize();
            // Mac has a little trouble with initial Layout() sizing...
            #ifdef __WXOSX__
                if (sz.x == 0) sz.IncTo(wxSize(160, 388));
            #endif
            wxAuiPaneInfo p = wxAuiPaneInfo().Name(cont->m_sName).Caption(cont->m_sCaption).CaptionVisible(false).TopDockable(
                !vertical).BottomDockable(!vertical).LeftDockable(vertical).RightDockable(vertical).MinSize(
                sz).BestSize(sz).FloatingSize(sz).FloatingPosition(100, 100).Float().Show(cont->m_bIsVisible).Gripper(false) ;
            
            m_pauimgr->AddPane(cont->m_pOdometerWindow, p);
                //wxAuiPaneInfo().Name(cont->m_sName).Caption(cont->m_sCaption).CaptionVisible(false).TopDockable(
               // !vertical).BottomDockable(!vertical).LeftDockable(vertical).RightDockable(vertical).MinSize(
               // sz).BestSize(sz).FloatingSize(sz).FloatingPosition(100, 100).Float().Show(cont->m_bIsVisible));

            #ifdef __OCPN__ANDROID__
            wxAuiPaneInfo& pane = m_pauimgr->GetPane( cont->m_pOdometerWindow );
            pane.Dockable( false );
            
            #endif            

        } else {  
            // Update the current odometer
            wxAuiPaneInfo& pane = m_pauimgr->GetPane(cont->m_pOdometerWindow);
            pane.Caption(cont->m_sCaption).Show(cont->m_bIsVisible);
            if (!cont->m_pOdometerWindow->isInstrumentListEqual(cont->m_aInstrumentList)) {
                cont->m_pOdometerWindow->SetInstrumentList(cont->m_aInstrumentList);
                wxSize sz = cont->m_pOdometerWindow->GetMinSize();
                pane.MinSize(sz).BestSize(sz).FloatingSize(sz);
            }
            if (cont->m_pOdometerWindow->GetSizerOrientation() != orient) {
                cont->m_pOdometerWindow->ChangePaneOrientation(orient, false);
            }
        }
    }
    m_pauimgr->Update();
}

void odometer_pi::PopulateContextMenu(wxMenu* menu) {
    OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(0);
    wxMenuItem* item = menu->AppendCheckItem(1, cont->m_sCaption);
    item->Check(cont->m_bIsVisible);
}

void odometer_pi::ShowOdometer(size_t id, bool visible) {
    if (id < m_ArrayOfOdometerWindow.GetCount()) {
        OdometerWindowContainer *cont = m_ArrayOfOdometerWindow.Item(id);
        m_pauimgr->GetPane(cont->m_pOdometerWindow).Show(visible);
        cont->m_bIsVisible = visible;
        cont->m_bPersVisible = visible;
        m_pauimgr->Update();
    }
}


//---------------------------------------------------------------------------------------------------------
//
// OdometerPreferencesDialog
//
//---------------------------------------------------------------------------------------------------------

OdometerPreferencesDialog::OdometerPreferencesDialog(wxWindow *parent, wxWindowID id, wxArrayOfOdometer config) :
        wxDialog(parent, id, _("Odometer Settings"), wxDefaultPosition, wxDefaultSize,  wxDEFAULT_DIALOG_STYLE) {
    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(OdometerPreferencesDialog::OnCloseDialog), NULL, this);

    // Copy original config
    m_Config = wxArrayOfOdometer(config);
    // Build Odometer Page for Toolbox
    int border_size = 2;

    wxBoxSizer* itemBoxSizerMainPanel = new wxBoxSizer(wxVERTICAL);
    SetSizer(itemBoxSizerMainPanel);

    wxFlexGridSizer *itemFlexGridSizer = new wxFlexGridSizer(2);
    itemFlexGridSizer->AddGrowableCol(1);
    m_pPanelPreferences = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN);
    itemBoxSizerMainPanel->Add(m_pPanelPreferences, 1, wxEXPAND | wxTOP | wxRIGHT, border_size);

    wxBoxSizer* itemBoxSizerMainFrame = new wxBoxSizer(wxVERTICAL);
    m_pPanelPreferences->SetSizer(itemBoxSizerMainFrame);

    wxStaticBox* itemStaticBoxDispOpts = new wxStaticBox(m_pPanelPreferences, wxID_ANY, _("Display options"));
    wxStaticBoxSizer* itemStaticBoxSizer03 = new wxStaticBoxSizer(itemStaticBoxDispOpts, wxHORIZONTAL);
    itemBoxSizerMainFrame->Add(itemStaticBoxSizer03, 1, wxEXPAND | wxALL, border_size);
    wxFlexGridSizer *itemFlexGridSizer01 = new wxFlexGridSizer(2);
    itemFlexGridSizer01->AddGrowableCol(0); 
    itemStaticBoxSizer03->Add(itemFlexGridSizer01, 1, wxEXPAND | wxALL, 0);

    m_pCheckBoxIsVisible = new wxCheckBox(m_pPanelPreferences, wxID_ANY, _("Show this odometer"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer01->Add(m_pCheckBoxIsVisible, 0, wxEXPAND | wxALL, border_size);

    m_pCheckBoxShowSpeed = new wxCheckBox(m_pPanelPreferences, wxID_ANY, _("Show Speedometer instrument"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer01->Add(m_pCheckBoxShowSpeed, 0, wxEXPAND | wxALL, border_size);

    m_pCheckBoxShowDepArrTimes = new wxCheckBox(m_pPanelPreferences, wxID_ANY, _("Show Dep. and Arr. times"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer01->Add(m_pCheckBoxShowDepArrTimes, 0, wxEXPAND | wxALL, border_size);

    m_pCheckBoxShowTripLeg = new wxCheckBox(m_pPanelPreferences, wxID_ANY, _("Show/Reset Leg Distance and time"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer01->Add(m_pCheckBoxShowTripLeg, 0, wxEXPAND | wxALL, border_size);

    /* There must be an even number of checkboxes/objects preceeding caption or alignment gets messed up,
       enable the next section as required  */
    /*
    wxStaticText *itemDummy01 = new wxStaticText(m_pPanelPreferences, wxID_ANY, _T(""));
       itemFlexGridSizer01->Add(itemDummy01, 0, wxEXPAND | wxALL, border_size);  
    */

    wxStaticText* itemStaticText01 = new wxStaticText(m_pPanelPreferences, wxID_ANY, _("Caption:"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer01->Add(itemStaticText01, 0, wxEXPAND | wxALL, border_size);
    m_pTextCtrlCaption = new wxTextCtrl(m_pPanelPreferences, wxID_ANY, _T(""), wxDefaultPosition,
            wxDefaultSize);
    itemFlexGridSizer01->Add(m_pTextCtrlCaption, 0, wxEXPAND | wxALL, border_size);

#ifdef __OCPN__ANDROID__
    itemStaticText01->Hide();
    m_pTextCtrlCaption->Hide();
#endif    
    wxStaticBox* itemStaticBoxFonts = new wxStaticBox( m_pPanelPreferences, wxID_ANY, _("Fonts") );
    wxStaticBoxSizer* itemStaticBoxSizer04 = new wxStaticBoxSizer( itemStaticBoxFonts, wxHORIZONTAL );
    itemBoxSizerMainFrame->Add( itemStaticBoxSizer04, 0, wxEXPAND | wxALL, border_size );
    wxFlexGridSizer *itemFlexGridSizer02 = new wxFlexGridSizer( 2 );
    itemFlexGridSizer02->AddGrowableCol( 1 );
    itemStaticBoxSizer04->Add( itemFlexGridSizer02, 1, wxEXPAND | wxALL, 0 );
    itemBoxSizerMainFrame->AddSpacer( 5 );

    wxStaticText* itemStaticText02 = new wxStaticText(m_pPanelPreferences, wxID_ANY, _("Title:"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer02->Add(itemStaticText02, 0, wxEXPAND | wxALL, border_size);
    m_pFontPickerTitle = new wxFontPickerCtrl(m_pPanelPreferences, wxID_ANY, *g_pFontTitle,
            wxDefaultPosition, wxDefaultSize);
    itemFlexGridSizer02->Add(m_pFontPickerTitle, 0, wxALIGN_RIGHT | wxALL, 0);

    wxStaticText* itemStaticText03 = new wxStaticText(m_pPanelPreferences, wxID_ANY, _("Data:"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer02->Add(itemStaticText03, 0, wxEXPAND | wxALL, border_size);
    m_pFontPickerData = new wxFontPickerCtrl(m_pPanelPreferences, wxID_ANY, *g_pFontData,
            wxDefaultPosition, wxDefaultSize);
    itemFlexGridSizer02->Add(m_pFontPickerData, 0, wxALIGN_RIGHT | wxALL, 0);

    wxStaticText* itemStaticText04 = new wxStaticText(m_pPanelPreferences, wxID_ANY, _("Label:"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer02->Add(itemStaticText04, 0, wxEXPAND | wxALL, border_size);
    m_pFontPickerLabel = new wxFontPickerCtrl(m_pPanelPreferences, wxID_ANY, *g_pFontLabel,
            wxDefaultPosition, wxDefaultSize);
    itemFlexGridSizer02->Add(m_pFontPickerLabel, 0, wxALIGN_RIGHT | wxALL, 0);

    wxStaticText* itemStaticText05 = new wxStaticText(m_pPanelPreferences, wxID_ANY, _("Small:"),
            wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer02->Add(itemStaticText05, 0, wxEXPAND | wxALL, border_size);
    m_pFontPickerSmall = new wxFontPickerCtrl(m_pPanelPreferences, wxID_ANY, *g_pFontSmall,
            wxDefaultPosition, wxDefaultSize);
    itemFlexGridSizer02->Add(m_pFontPickerSmall, 0, wxALIGN_RIGHT | wxALL, 0);
	
    wxStaticBox* itemStaticBoxURF = new wxStaticBox( m_pPanelPreferences, wxID_ANY, 
    _("Units, Ranges, Formats") );
    wxStaticBoxSizer* itemStaticBoxSizer05 = new wxStaticBoxSizer( itemStaticBoxURF, wxHORIZONTAL );
    itemBoxSizerMainFrame->Add( itemStaticBoxSizer05, 0, wxEXPAND | wxALL, border_size );
    wxFlexGridSizer *itemFlexGridSizer03 = new wxFlexGridSizer( 2 );
    itemFlexGridSizer03->AddGrowableCol( 1 );
    itemStaticBoxSizer05->Add( itemFlexGridSizer03, 1, wxEXPAND | wxALL, 0 );
    itemBoxSizerMainFrame->AddSpacer( 5 );
 
    wxStaticText* itemStaticText06 = new wxStaticText( m_pPanelPreferences, wxID_ANY, _("Speedometer max value:"), 
        wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer03->Add( itemStaticText06, 0, wxEXPAND | wxALL, border_size );
    m_pSpinSpeedMax = new wxSpinCtrl( m_pPanelPreferences, wxID_ANY, wxEmptyString, wxDefaultPosition, 
        wxDefaultSize, wxSP_ARROW_KEYS, 10, 80, g_iOdoSpeedMax );
    itemFlexGridSizer03->Add(m_pSpinSpeedMax, 0, wxALIGN_RIGHT | wxALL, 0);

    wxStaticText* itemStaticText07 = new wxStaticText( m_pPanelPreferences, wxID_ANY, _("Minimum On-Route speed:"), 
        wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizer03->Add(itemStaticText07, 0, wxEXPAND | wxALL, border_size);
    m_pSpinOnRoute = new wxSpinCtrl(m_pPanelPreferences, wxID_ANY, wxEmptyString, wxDefaultPosition, 
        wxDefaultSize, wxSP_ARROW_KEYS, 0, 5, g_iOdoOnRoute);
    itemFlexGridSizer03->Add(m_pSpinOnRoute, 0, wxALIGN_RIGHT | wxALL, 0);

    wxStaticText* itemStaticText11 = new wxStaticText( m_pPanelPreferences, wxID_ANY, _( "Local Time Offset From UTC:" ), 
        wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer03->Add( itemStaticText11, 0, wxEXPAND | wxALL, border_size );
    wxString m_UTCOffsetChoices[] = {
        _T( "-12:00" ), _T( "-11:30" ), _T( "-11:00" ), _T( "-10:30" ), _T( "-10:00" ), _T( "-09:30" ),
        _T( "-09:00" ), _T( "-08:30" ), _T( "-08:00" ), _T( "-07:30" ), _T( "-07:00" ), _T( "-06:30" ),
        _T( "-06:00" ), _T( "-05:30" ), _T( "-05:00" ), _T( "-04:30" ), _T( "-04:00" ), _T( "-03:30" ),
        _T( "-03:00" ), _T( "-02:30" ), _T( "-02:00" ), _T( "-01:30" ), _T( "-01:00" ), _T( "-00:30" ),
        _T( " 00:00" ), _T( " 00:30" ), _T( " 01:00" ), _T( " 01:30" ), _T( " 02:00" ), _T( " 02:30" ),
        _T( " 03:00" ), _T( " 03:30" ), _T( " 04:00" ), _T( " 04:30" ), _T( " 05:00" ), _T( " 05:30" ),
        _T( " 06:00" ), _T( " 06:30" ), _T( " 07:00" ), _T( " 07:30" ), _T( " 08:00" ), _T( " 08:30" ),
        _T( " 09:00" ), _T( " 09:30" ), _T( " 10:00" ), _T( " 10:30" ), _T( " 11:00" ), _T( " 11:30" ),
        _T( " 12:00" )
    };
    int m_UTCOffsetNChoices = sizeof( m_UTCOffsetChoices ) / sizeof( wxString );
    m_pChoiceUTCOffset = new wxChoice( m_pPanelPreferences, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
        m_UTCOffsetNChoices, m_UTCOffsetChoices, 0 );
    m_pChoiceUTCOffset->SetSelection( g_iOdoUTCOffset );
    itemFlexGridSizer03->Add( m_pChoiceUTCOffset, 0, wxALIGN_RIGHT | wxALL, 0 );

    wxStaticText* itemStaticText12 = new wxStaticText( m_pPanelPreferences, wxID_ANY, _("Boat speed units:"), 
        wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer03->Add( itemStaticText12, 0, wxEXPAND | wxALL, border_size );
    wxString m_SpeedUnitChoices[] = { _("Kts"), _("mph"), _("km/h"), _("m/s") };
    int m_SpeedUnitNChoices = sizeof( m_SpeedUnitChoices ) / sizeof( wxString );
    m_pChoiceSpeedUnit = new wxChoice( m_pPanelPreferences, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
        m_SpeedUnitNChoices, m_SpeedUnitChoices, 0 );
    m_pChoiceSpeedUnit->SetSelection( g_iOdoSpeedUnit );
    itemFlexGridSizer03->Add( m_pChoiceSpeedUnit, 0, wxALIGN_RIGHT | wxALL, 0 );

    wxStaticText* itemStaticText13 = new wxStaticText( m_pPanelPreferences, wxID_ANY, _("Distance units:"), 
        wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer03->Add( itemStaticText13, 0, wxEXPAND | wxALL, border_size );
    wxString m_DistanceUnitChoices[] = { _("Nautical miles"), _("Statute miles"), _("Kilometers") };
    int m_DistanceUnitNChoices = sizeof( m_DistanceUnitChoices ) / sizeof( wxString );
    m_pChoiceDistanceUnit = new wxChoice( m_pPanelPreferences, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
        m_DistanceUnitNChoices, m_DistanceUnitChoices, 0 );
    m_pChoiceDistanceUnit->SetSelection( g_iOdoDistanceUnit );
    itemFlexGridSizer03->Add( m_pChoiceDistanceUnit, 0, wxALIGN_RIGHT | wxALL, 0 );

	wxStdDialogButtonSizer* DialogButtonSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    itemBoxSizerMainPanel->Add(DialogButtonSizer, 0, wxALIGN_RIGHT | wxALL, 5);

    /* NOTE: These are not preferences settings items, there are no change options in Odometer 
             besides the ones used when toggling show checkboxes. */ 
    m_pListCtrlOdometers = new wxListCtrl( this , wxID_ANY, wxDefaultPosition, wxSize(0, 0),
         wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL);
    m_pListCtrlInstruments = new wxListCtrl( this, wxID_ANY, wxDefaultPosition, wxSize( 0, 0 ),
         wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING );
    m_pListCtrlInstruments->InsertColumn(0, _("Instruments"));


    UpdateOdometerButtonsState();
//    SetMinSize(wxSize(450, -1));
    SetMinSize(wxSize(200, -1));
    Fit();
}

void OdometerPreferencesDialog::RecalculateSize( void )
{

#ifdef __OCPN__ANDROID__    
    wxSize esize;
    esize.x = GetCharWidth() * 110;
    esize.y = GetCharHeight() * 40;
    
    wxSize dsize = GetOCPNCanvasWindow()->GetClientSize(); 
    esize.y = wxMin( esize.y, dsize.y -(3 * GetCharHeight()) );
    esize.x = wxMin( esize.x, dsize.x -(3 * GetCharHeight()) );
    SetSize(esize);

    CentreOnScreen();
#endif
    
}

void OdometerPreferencesDialog::OnCloseDialog(wxCloseEvent& event) {

    SaveOdometerConfig();
    event.Skip();
}

void OdometerPreferencesDialog::SaveOdometerConfig(void) {
    
    g_iOdoSpeedMax = m_pSpinSpeedMax->GetValue();  
    g_iOdoOnRoute = m_pSpinOnRoute->GetValue(); 
    g_iOdoUTCOffset = m_pChoiceUTCOffset->GetSelection();
    g_iOdoSpeedUnit = m_pChoiceSpeedUnit->GetSelection();
    g_iOdoDistanceUnit = m_pChoiceDistanceUnit->GetSelection();

    OdometerWindowContainer *cont = m_Config.Item(0);
    cont->m_bIsVisible = m_pCheckBoxIsVisible->IsChecked();
    cont->m_bShowSpeed = m_pCheckBoxShowSpeed->IsChecked();
    cont->m_bShowDepArrTimes = m_pCheckBoxShowDepArrTimes->IsChecked();
    cont->m_bShowTripLeg = m_pCheckBoxShowTripLeg->IsChecked();
    cont->m_sCaption = m_pTextCtrlCaption->GetValue();

    /* Do not regenreate the array, reorders the instruments on Windows (only!)
    cont->m_aInstrumentList.Clear();
    for (int i = 0; i < m_pListCtrlInstruments->GetItemCount(); i++)
        cont->m_aInstrumentList.Add((int) m_pListCtrlInstruments->GetItemData(i));
     */
}

void OdometerPreferencesDialog::OnOdometerSelected(wxListEvent& event) {
    SaveOdometerConfig();
    UpdateOdometerButtonsState();
}

void OdometerPreferencesDialog::UpdateOdometerButtonsState() {
    long item = -1;

    // Forcing 'item = 0' enables the one (and only) panel in the settings dialogue.
    // item = m_pListCtrlOdometers->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    item = 0; 

    bool enable = (item != -1);

    m_pPanelPreferences->Enable( enable );

    OdometerWindowContainer *cont = m_Config.Item(0);
    m_pCheckBoxIsVisible->SetValue(cont->m_bIsVisible);
    m_pCheckBoxShowSpeed->SetValue(cont->m_bShowSpeed);
    m_pCheckBoxShowDepArrTimes->SetValue(cont->m_bShowDepArrTimes);
    m_pCheckBoxShowTripLeg->SetValue(cont->m_bShowTripLeg);
    m_pTextCtrlCaption->SetValue(cont->m_sCaption);
    m_pListCtrlInstruments->DeleteAllItems();
    for (size_t i = 0; i < cont->m_aInstrumentList.GetCount(); i++) {
        wxListItem item;
        GetListItemForInstrument(item, cont->m_aInstrumentList.Item(i));
        item.SetId(m_pListCtrlInstruments->GetItemCount());
        m_pListCtrlInstruments->InsertItem(item);
    }
    m_pListCtrlInstruments->SetColumnWidth(0, wxLIST_AUTOSIZE);
}


//---------------------------------------------------------------------------------------------------------
//
//    Odometer Window Implementation
//
//---------------------------------------------------------------------------------------------------------

// wxWS_EX_VALIDATE_RECURSIVELY required to push events to parents
OdometerWindow::OdometerWindow(wxWindow *pparent, wxWindowID id, wxAuiManager *auimgr,
        odometer_pi* plugin, int orient, OdometerWindowContainer* mycont) :
        wxWindow(pparent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, _T("Odometer")) {
    m_pauimgr = auimgr;
    m_plugin = plugin;
    m_Container = mycont;

	// wx2.9 itemBoxSizer = new wxWrapSizer(orient);
    itemBoxSizer = new wxBoxSizer(orient);
    SetSizer(itemBoxSizer);
    Connect(wxEVT_SIZE, wxSizeEventHandler(OdometerWindow::OnSize), NULL, this);
    Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(OdometerWindow::OnContextMenu), NULL,
            this);
    Connect(wxEVT_COMMAND_MENU_SELECTED,
            wxCommandEventHandler(OdometerWindow::OnContextMenuSelect), NULL, this);


#ifdef __OCPN__ANDROID__ 
    Connect( wxEVT_LEFT_DOWN, wxMouseEventHandler( OdometerWindow::OnMouseEvent ) );
    Connect( wxEVT_LEFT_UP, wxMouseEventHandler( Odometerindow::OnMouseEvent ) );
    Connect( wxEVT_MOTION, wxMouseEventHandler( OdometerWindow::OnMouseEvent ) );
    
    GetHandle()->setAttribute(Qt::WA_AcceptTouchEvents);
    GetHandle()->grabGesture(Qt::PinchGesture);
    GetHandle()->grabGesture(Qt::PanGesture);
    
    Connect( wxEVT_QT_PINCHGESTURE,
            (wxObjectEventFunction) (wxEventFunction) &OdometerWindow::OnEvtPinchGesture, NULL, this );
    Connect( wxEVT_QT_PANGESTURE,
             (wxObjectEventFunction) (wxEventFunction) &OdometerWindow::OnEvtPanGesture, NULL, this );
#endif
    
    Hide();
    
    m_binResize = false;
    m_binPinch = false;
    
}

OdometerWindow::~OdometerWindow() {
    for (size_t i = 0; i < m_ArrayOfInstrument.GetCount(); i++) {
        OdometerInstrumentContainer *pdic = m_ArrayOfInstrument.Item(i);
        delete pdic;
    }
}


#ifdef __OCPN__ANDROID__
void OdometerWindow::OnEvtPinchGesture( wxQT_PinchGestureEvent &event)
{
    
    float zoom_gain = 0.3;
    float zoom_val;
    float total_zoom_val;
    
    if( event.GetScaleFactor() > 1)
        zoom_val = ((event.GetScaleFactor() - 1.0) * zoom_gain) + 1.0;
    else
        zoom_val = 1.0 - ((1.0 - event.GetScaleFactor()) * zoom_gain);
    
    if( event.GetTotalScaleFactor() > 1)
        total_zoom_val = ((event.GetTotalScaleFactor() - 1.0) * zoom_gain) + 1.0;
    else
        total_zoom_val = 1.0 - ((1.0 - event.GetTotalScaleFactor()) * zoom_gain);
    

    wxAuiPaneInfo& pane = m_pauimgr->GetPane( this );
    
    wxSize currentSize = wxSize( pane.floating_size.x, pane.floating_size.y );
    double aRatio = (double)currentSize.y / (double)currentSize.x;

    wxSize par_size = GetOCPNCanvasWindow()->GetClientSize();
    wxPoint par_pos = wxPoint( pane.floating_pos.x, pane.floating_pos.y );
    
    switch(event.GetState()){
        case GestureStarted:
            m_binPinch = true;
            break;
            
        case GestureUpdated:
            currentSize.y *= zoom_val;
            currentSize.x *= zoom_val;

            if((par_pos.y + currentSize.y) > par_size.y)
                currentSize.y = par_size.y - par_pos.y;
            
            if((par_pos.x + currentSize.x) > par_size.x)
                currentSize.x = par_size.x - par_pos.x;
            
            
            ///vertical
            currentSize.x = currentSize.y / aRatio;
                
            currentSize.x = wxMax(currentSize.x, 150);
            currentSize.y = wxMax(currentSize.y, 150);
            
            pane.FloatingSize(currentSize);
            m_pauimgr->Update();
            
            
            break;
            
        case GestureFinished:{

            if(itemBoxSizer->GetOrientation() == wxVERTICAL){
                currentSize.y *= total_zoom_val;
                currentSize.x = currentSize.y / aRatio;
            }
            else{
                currentSize.x *= total_zoom_val;
                currentSize.y = currentSize.x * aRatio;
            }
            
            
            //  Bound the resulting size
            if((par_pos.y + currentSize.y) > par_size.y)
                currentSize.y = par_size.y - par_pos.y;
            
            if((par_pos.x + currentSize.x) > par_size.x)
                currentSize.x = par_size.x - par_pos.x;
 
            // not too small
            currentSize.x = wxMax(currentSize.x, 150);
            currentSize.y = wxMax(currentSize.y, 150);
                
            //  Try a manual layout of the window, to estimate a good primary size..

            // vertical
            if(itemBoxSizer->GetOrientation() == wxVERTICAL){
                int total_y = 0;
                for( unsigned int i=0; i<m_ArrayOfInstrument.size(); i++ ) {
                    OdometerInstrument* inst = m_ArrayOfInstrument.Item(i)->m_pInstrument;
                    wxSize is = inst->GetSize( itemBoxSizer->GetOrientation(), currentSize );
                    total_y += is.y;
                }
        
                currentSize.y = total_y;
            }
    
    
            pane.FloatingSize(currentSize);
            
            // Reshow the window
            for( unsigned int i=0; i<m_ArrayOfInstrument.size(); i++ ) {
                OdometerInstrument* inst = m_ArrayOfInstrument.Item(i)->m_pInstrument;
                inst->Show();
            }
            
            m_pauimgr->Update();
            
            m_binPinch = false;
            m_binResize = false;
            
            break;
        }
        
        case GestureCanceled:
            m_binPinch = false;
            m_binResize = false;
            break;
            
        default:
            break;
    }
    
}


void OdometerWindow::OnEvtPanGesture( wxQT_PanGestureEvent &event)
{
    if(m_binPinch)
        return;

    if(m_binResize)
        return;
        
    int x = event.GetOffset().x;
    int y = event.GetOffset().y;
    
    int lx = event.GetLastOffset().x;
    int ly = event.GetLastOffset().y;
    
    int dx = x - lx;
    int dy = y - ly;
    
    switch(event.GetState()){
        case GestureStarted:
            if(m_binPan)
                break;
            
            m_binPan = true;
            break;
            
        case GestureUpdated:
            if(m_binPan){
                
                wxSize par_size = GetOCPNCanvasWindow()->GetClientSize();
                wxPoint par_pos_old = ClientToScreen( wxPoint( 0, 0 ) ); //GetPosition();
                
                wxPoint par_pos = par_pos_old;
                par_pos.x += dx;
                par_pos.y += dy;
                
                par_pos.x = wxMax(par_pos.x, 0);
                par_pos.y = wxMax(par_pos.y, 0);
                
                wxSize mySize = GetSize();
                
                if((par_pos.y + mySize.y) > par_size.y)
                    par_pos.y = par_size.y - mySize.y;
                
                
                if((par_pos.x + mySize.x) > par_size.x)
                    par_pos.x = par_size.x - mySize.x;
                
                wxAuiPaneInfo& pane = m_pauimgr->GetPane( this );
                pane.FloatingPosition( par_pos).Float();
                m_pauimgr->Update();
                
            }
            break;
            
        case GestureFinished:
            if(m_binPan){
            }
            m_binPan = false;
            
            break;
            
        case GestureCanceled:
            m_binPan = false; 
            break;
            
        default:
            break;
    }
    
    
}
    
    
void OdometerWindow::OnMouseEvent( wxMouseEvent& event )
{
    if(m_binPinch)
        return;

    if(m_binResize){
        
        wxAuiPaneInfo& pane = m_pauimgr->GetPane( this );
        wxSize currentSize = wxSize( pane.floating_size.x, pane.floating_size.y );
        double aRatio = (double)currentSize.y / (double)currentSize.x;
        
        wxSize par_size = GetOCPNCanvasWindow()->GetClientSize();
        wxPoint par_pos = wxPoint( pane.floating_pos.x, pane.floating_pos.y );
        
        if(event.LeftDown()){
            m_resizeStartPoint = event.GetPosition();
            m_resizeStartSize = currentSize;
            m_binResize2 = true;
         }

        if(m_binResize2){ 
            if(event.Dragging()){
                wxPoint p = event.GetPosition();
                
                wxSize dragSize = m_resizeStartSize;
                
                dragSize.y += p.y - m_resizeStartPoint.y;
                dragSize.x += p.x - m_resizeStartPoint.x;;

                if((par_pos.y + dragSize.y) > par_size.y)
                    dragSize.y = par_size.y - par_pos.y;
                
                if((par_pos.x + dragSize.x) > par_size.x)
                    dragSize.x = par_size.x - par_pos.x;
                
                
                ///vertical
                //dragSize.x = dragSize.y / aRatio;

                // not too small
                dragSize.x = wxMax(dragSize.x, 150);
                dragSize.y = wxMax(dragSize.y, 150);
                
                pane.FloatingSize(dragSize);
                m_pauimgr->Update();
                    
            }
            
            if(event.LeftUp()){
                wxPoint p = event.GetPosition();
                
                wxSize dragSize = m_resizeStartSize;
                
                dragSize.y += p.y - m_resizeStartPoint.y;
                dragSize.x += p.x - m_resizeStartPoint.x;;

                if((par_pos.y + dragSize.y) > par_size.y)
                    dragSize.y = par_size.y - par_pos.y;
                
                if((par_pos.x + dragSize.x) > par_size.x)
                    dragSize.x = par_size.x - par_pos.x;

                // not too small
                dragSize.x = wxMax(dragSize.x, 150);
                dragSize.y = wxMax(dragSize.y, 150);
/*
                for( unsigned int i=0; i<m_ArrayOfInstrument.size(); i++ ) {
                    OdometerInstrument* inst = m_ArrayOfInstrument.Item(i)->m_pInstrument;
                    inst->Show();
                }
*/
                pane.FloatingSize(dragSize);
                m_pauimgr->Update();
                
                
                m_binResize = false;
                m_binResize2 = false;
            }
        }
    }
}
#endif


void OdometerWindow::OnSize(wxSizeEvent& event) {
    event.Skip();
    for (unsigned int i=0; i<m_ArrayOfInstrument.size(); i++) {
        OdometerInstrument* inst = m_ArrayOfInstrument.Item(i)->m_pInstrument;
        inst->SetMinSize(inst->GetSize(itemBoxSizer->GetOrientation(), GetClientSize()));
    }
    // TODO: Better handling of size after repetitive closing of preferences (almost ok)
    SetMinSize(wxDefaultSize);
    Fit();
    SetMinSize(itemBoxSizer->GetMinSize());
    Layout();
    Refresh();
}

void OdometerWindow::OnContextMenu(wxContextMenuEvent& event) {
    wxMenu* contextMenu = new wxMenu();

    wxAuiPaneInfo &pane = m_pauimgr->GetPane(this);
    if (pane.IsOk() && pane.IsDocked()) {
        contextMenu->Append(ID_ODO_UNDOCK, _("Undock"));
    }
    contextMenu->Append(ID_ODO_PREFS, _("Preferences ..."));
    PopupMenu(contextMenu);
    delete contextMenu;
}

void OdometerWindow::OnContextMenuSelect(wxCommandEvent& event) {
    if (event.GetId() < ID_ODO_PREFS) { 
	// Toggle odometer visibility
        m_plugin->ShowOdometer(event.GetId()-1, event.IsChecked());
        SetToolbarItemState(m_plugin->GetToolbarItemId(), m_plugin->GetOdometerWindowShownCount() != 0);
    }

    switch(event.GetId()) {
        case ID_ODO_PREFS: {
            m_plugin->ShowPreferencesDialog(this);
            return; // Does it's own save.
        }

        case ID_ODO_UNDOCK: {
            ChangePaneOrientation(GetSizerOrientation(), true);
            return;     // Nothing changed so nothing need be saved
        }
    }
    
    m_plugin->SaveConfig();
}

void OdometerWindow::SetColorScheme(PI_ColorScheme cs) {
    DimeWindow(this);
    
    // Improve appearance, especially in DUSK or NIGHT palette
    wxColour col;
    GetGlobalColor(_T("DASHL"), &col);
    SetBackgroundColour(col);
    Refresh(false);
}

void OdometerWindow::ChangePaneOrientation(int orient, bool updateAUImgr) {
    m_pauimgr->DetachPane(this);
    SetSizerOrientation(orient);
    bool vertical = orient == wxVERTICAL;
    wxSize sz = GetMinSize();

    // We must change Name to reset AUI perpective
    m_Container->m_sName = MakeName();
    m_pauimgr->AddPane(this, wxAuiPaneInfo().Name(m_Container->m_sName).Caption(
        m_Container->m_sCaption).CaptionVisible(true).TopDockable(!vertical).BottomDockable(
        !vertical).LeftDockable(vertical).RightDockable(vertical).MinSize(sz).BestSize(
        sz).FloatingSize(sz).FloatingPosition(100, 100).Float().Show(m_Container->m_bIsVisible));

    
#ifdef __OCPN__ANDROID__
    wxAuiPaneInfo& pane = m_pauimgr->GetPane( this );
    pane.Dockable( false );
#endif            

    if (updateAUImgr) m_pauimgr->Update();
}

void OdometerWindow::SetSizerOrientation(int orient) {
    itemBoxSizer->SetOrientation(orient);
    // We must reset all MinSize to ensure we start with new default
    wxWindowListNode* node = GetChildren().GetFirst();
    while(node) {
        node->GetData()->SetMinSize(wxDefaultSize);
        node = node->GetNext();
    }
    SetMinSize(wxDefaultSize);
    Fit();
    SetMinSize(itemBoxSizer->GetMinSize());
}

int OdometerWindow::GetSizerOrientation() {
    return itemBoxSizer->GetOrientation();
}

bool isArrayIntEqual(const wxArrayInt& l1, const wxArrayOfInstrument &l2) {
    if (l1.GetCount() != l2.GetCount()) return false;

    for (size_t i = 0; i < l1.GetCount(); i++)
        if (l1.Item(i) != l2.Item(i)->m_ID) return false;

    return true;
}

bool OdometerWindow::isInstrumentListEqual(const wxArrayInt& list) {
    return isArrayIntEqual(list, m_ArrayOfInstrument);
}

// Create and display each instrument in a odometer container
void OdometerWindow::SetInstrumentList(wxArrayInt list) {

    m_ArrayOfInstrument.Clear();
    itemBoxSizer->Clear(true);

    for (size_t i = 0; i < list.GetCount(); i++) {

        int id = list.Item(i);
        OdometerInstrument *instrument = NULL;

        switch (id) {

            case ID_DBP_D_SOG:
                if ( g_iShowSpeed == 1 ) { 
                    instrument = new OdometerInstrument_Speedometer( this, wxID_ANY,
                        GetInstrumentCaption( id ), OCPN_DBP_STC_SOG, 0, g_iOdoSpeedMax );
                    ( (OdometerInstrument_Dial *) instrument )->SetOptionLabel
                        ( g_iOdoSpeedMax / 20 + 1, DIAL_LABEL_HORIZONTAL );
                    ( (OdometerInstrument_Dial *) instrument )->SetOptionMarker( 0.5, DIAL_MARKER_SIMPLE, 2 );
                }
                break;

            case ID_DBP_I_SUMLOG:
                instrument = new OdometerInstrument_Single( this, wxID_ANY,
                    GetInstrumentCaption( id ), OCPN_DBP_STC_SUMLOG, _T("%14.1f") );
                break;

            case ID_DBP_I_TRIPLOG:
                instrument = new OdometerInstrument_Single( this, wxID_ANY,
                    GetInstrumentCaption( id ), OCPN_DBP_STC_TRIPLOG, _T("%14.1f") );
                break;

            case ID_DBP_I_DEPART:
                if ( g_iShowDepArrTimes == 1 ) { 
                    instrument = new OdometerInstrument_String( this, wxID_ANY,
                        GetInstrumentCaption( id ), OCPN_DBP_STC_DEPART, _T("%1s") );
                }
                break;

            case ID_DBP_I_ARRIV:
                if ( g_iShowDepArrTimes == 1 ) { 
                    instrument = new OdometerInstrument_String( this, wxID_ANY,
                        GetInstrumentCaption( id ), OCPN_DBP_STC_ARRIV, _T("%1s") );
                }
                break;

            case ID_DBP_B_TRIPRES:
                instrument = new OdometerInstrument_Button( this, wxID_ANY,
                    GetInstrumentCaption( id ), OCPN_DBP_STC_TRIPRES );
                break;

            case ID_DBP_I_LEGDIST:
                if ( g_iShowTripLeg == 1 ) { 
                    instrument = new OdometerInstrument_Single( this, wxID_ANY,
                        GetInstrumentCaption( id ), OCPN_DBP_STC_LEGDIST,_T("%14.2f") );
                }
                break;

            case ID_DBP_I_LEGTIME:
                if ( g_iShowTripLeg == 1 ) { 
                    instrument = new OdometerInstrument_String( this, wxID_ANY,
                        GetInstrumentCaption( id ), OCPN_DBP_STC_LEGTIME,_T("%8s") ); 
                }
                break;

            case ID_DBP_B_LEGRES:
                if ( g_iShowTripLeg == 1 ) { 
                    instrument = new OdometerInstrument_Button( this, wxID_ANY,
                        GetInstrumentCaption( id ), OCPN_DBP_STC_LEGRES );
                }
                break;
	    	}
        if (instrument) {
            instrument->instrumentTypeId = id;
            m_ArrayOfInstrument.Add(new OdometerInstrumentContainer(id, instrument,instrument->GetCapacity()));
            itemBoxSizer->Add(instrument, 0, wxEXPAND, 0);
        }
    }

    // Reset MinSize to ensure we start with a new default
    SetMinSize(wxDefaultSize);
    Fit();
    Layout();
    SetMinSize(itemBoxSizer->GetMinSize());
}

void OdometerWindow::SendSentenceToAllInstruments(int st, double value, wxString unit) {
    for (size_t i = 0; i < m_ArrayOfInstrument.GetCount(); i++) {
		if (m_ArrayOfInstrument.Item(i)->m_cap_flag & st) {
			m_ArrayOfInstrument.Item(i)->m_pInstrument->SetData(st, value, unit);
		}
    }
}
