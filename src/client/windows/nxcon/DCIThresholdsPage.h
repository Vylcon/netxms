#if !defined(AFX_DCITHRESHOLDSPAGE_H__15B4CFCB_DE84_406D_A98F_1ABE35E9965D__INCLUDED_)
#define AFX_DCITHRESHOLDSPAGE_H__15B4CFCB_DE84_406D_A98F_1ABE35E9965D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DCIThresholdsPage.h : header file
//

#include "ThresholdDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CDCIThresholdsPage dialog

class CDCIThresholdsPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CDCIThresholdsPage)

// Construction
public:
   NXC_DCI *m_pItem;
	CDCIThresholdsPage();
	~CDCIThresholdsPage();

// Dialog Data
	//{{AFX_DATA(CDCIThresholdsPage)
	enum { IDD = IDD_DCI_THRESHOLDS };
	CButton	m_wndButtonDelete;
	CButton	m_wndButtonModify;
	CButton	m_wndButtonDown;
	CButton	m_wndButtonUp;
	CListCtrl	m_wndListCtrl;
	CString	m_strInstance;
	BOOL	m_bAllThresholds;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CDCIThresholdsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void RecalcIndexes(DWORD dwIndex);
	void UpdateListEntry(int iItem, DWORD dwIndex);
	BOOL EditThreshold(NXC_DCI_THRESHOLD *pThreshold);
	int AddListEntry(DWORD dwIndex);
	// Generated message map functions
	//{{AFX_MSG(CDCIThresholdsPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonAdd();
	afx_msg void OnButtonModify();
	afx_msg void OnButtonDelete();
	afx_msg void OnButtonMoveup();
	afx_msg void OnButtonMovedown();
	afx_msg void OnItemchangedListThresholds(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkListThresholds(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DCITHRESHOLDSPAGE_H__15B4CFCB_DE84_406D_A98F_1ABE35E9965D__INCLUDED_)