// DCITransformPage.cpp : implementation file
//

#include "stdafx.h"
#include "nxcon.h"
#include "DCITransformPage.h"
#include "TransformTestDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//
// Delta calculation methods
//

static TCHAR *m_pszMethodList[] =
{
   _T("None - Keep original value"),
   _T("Simple delta"),
   _T("Average delta per second"),
   _T("Average delta per minute"),
   NULL
};


/////////////////////////////////////////////////////////////////////////////
// CDCITransformPage property page

IMPLEMENT_DYNCREATE(CDCITransformPage, CPropertyPage)

CDCITransformPage::CDCITransformPage() : CPropertyPage(CDCITransformPage::IDD)
{
	//{{AFX_DATA_INIT(CDCITransformPage)
	m_iDeltaProc = -1;
	//}}AFX_DATA_INIT
}

CDCITransformPage::~CDCITransformPage()
{
   m_wndEditScript.Detach();
}

void CDCITransformPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDCITransformPage)
	DDX_Control(pDX, IDC_COMBO_DELTA, m_wndDeltaList);
	DDX_CBIndex(pDX, IDC_COMBO_DELTA, m_iDeltaProc);
	//}}AFX_DATA_MAP

   if (pDX->m_bSaveAndValidate)
   {
      m_wndEditScript.GetText(m_strFormula);
   }
}


BEGIN_MESSAGE_MAP(CDCITransformPage, CPropertyPage)
	//{{AFX_MSG_MAP(CDCITransformPage)
	ON_CBN_SELCHANGE(IDC_COMBO_DELTA, OnSelchangeComboDelta)
	ON_BN_CLICKED(IDC_BUTTON_TEST, OnButtonTest)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDCITransformPage message handlers

BOOL CDCITransformPage::OnInitDialog() 
{
   int i;

	CPropertyPage::OnInitDialog();

   // Fill in delta calculation methods list
   for(i = 0; m_pszMethodList[i] != NULL; i++)
      m_wndDeltaList.AddString(m_pszMethodList[i]);
   m_wndDeltaList.SelectString(-1, m_pszMethodList[m_iDeltaProc]);

   m_wndEditScript.Attach(::GetDlgItem(m_hWnd, IDC_EDIT_SCRIPT));
   m_wndEditScript.SetDefaults();
   m_wndEditScript.SetText((LPCTSTR)m_strFormula);
   m_wndEditScript.LoadLexer("nxlexer.dll");
   m_wndEditScript.SetLexer("nxsl");
   m_wndEditScript.SetKeywords(0, g_szScriptKeywords);

   EnableWarning();
	return TRUE;
}


//
// Show or hide warning message
//

void CDCITransformPage::EnableWarning(void)
{
   TCHAR szBuffer[256];
   int nShow;

   // Get current polling interval from "Collection" page
   ((CPropertySheet *)GetParent())->GetPage(0)->GetDlgItemText(IDC_EDIT_INTERVAL, szBuffer, 256);

   // Enable or disable warning message
   nShow = ((m_iDeltaProc == DCM_AVERAGE_PER_MINUTE) && (_tcstol(szBuffer, NULL, 10) < 60)) ? SW_SHOWNA : SW_HIDE;
   ::ShowWindow(::GetDlgItem(m_hWnd, IDC_STATIC_WARNING_ICON), nShow);
   ::ShowWindow(::GetDlgItem(m_hWnd, IDC_STATIC_WARNING_TEXT), nShow);
}


//
// Page activation handler
//

BOOL CDCITransformPage::OnSetActive() 
{
   EnableWarning();
	return CPropertyPage::OnSetActive();
}


//
// Process delta calculation method selection change
//

void CDCITransformPage::OnSelchangeComboDelta() 
{
   TCHAR szBuffer[256];

   m_wndDeltaList.GetWindowText(szBuffer, 256);
   for(m_iDeltaProc = 0; m_pszMethodList[m_iDeltaProc] != NULL; m_iDeltaProc++)
      if (!_tcsicmp(m_pszMethodList[m_iDeltaProc], szBuffer))
         break;
   EnableWarning();
}


//
// Handler for "Test" button
//

void CDCITransformPage::OnButtonTest() 
{
	CTransformTestDlg dlg;

	m_wndEditScript.GetText(dlg.m_strScript);
	if (dlg.m_strScript.IsEmpty())
	{
		MessageBox(_T("Cannot test empty script"), _T("Warning"), MB_OK | MB_ICONEXCLAMATION);
	}
	else
	{
		dlg.m_dwNodeId = m_dwNodeId;
		dlg.m_dwItemId = m_dwItemId;
		dlg.DoModal();
	}
}