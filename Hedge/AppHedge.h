/************************************************************************
 * Copyright(c) 2011, One Unified. All rights reserved.                 *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

#pragma once

#include <wx/wx.h>

#include "ThreadMain.h"

class AppHedge : public wxApp {
  virtual bool OnInit();
  virtual int OnExit();
protected:
private:
  ThreadMain m_ThreadMain;
};
 
// Implements MyApp& wxGetApp()
DECLARE_APP(AppHedge)