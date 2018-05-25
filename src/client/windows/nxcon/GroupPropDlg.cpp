// GroupPropDlg.cpp : implementation file
//

#include "stdafx.h"
#include "nxcon.h"
#include "GroupPropDlg.h"
#include "UserSelectDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGroupPropDlg dialog


CGroupPropDlg::CGroupPropDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CGroupPropDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGroupPropDlg)
	m_bDisabled = FALSE;
	m_bDropConn = FALSE;
	m_bEditEventDB = FALSE;
	m_bManageUsers = FALSE;
	m_bViewEventDB = FALSE;
	m_strDescription = _T("");
	m_strName = _T("");
	m_bManageActions = FALSE;
	m_bManageEPP = FALSE;
	m_bManageConfig = FALSE;
	m_bConfigureTraps = FALSE;
	m_bManagePkg = FALSE;
	m_bDeleteAlarms = FALSE;
	m_bManageAgentCfg = FALSE;
	m_bManageScripts = FALSE;
	m_bManageTools = FALSE;
	m_bViewTrapLog = FALSE;
	m_bAccessFiles = FALSE;
	m_bRegisterAgents = FALSE;
	m_bManageMaps = FALSE;
	m_bManageSituations = FALSE;
	//}}AFX_DATA_INIT

   m_pdwMembers = NULL;
}


CGroupPropDlg::~CGroupPropDlg()
{
   safe_free(m_pdwMembers);
}


void CGroupPropDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGroupPropDlg)
	DDX_Control(pDX, IDC_LIST_MEMBERS, m_wndListCtrl);
	DDX_Check(pDX, IDC_CHECK_DISABLED, m_bDisabled);
	DDX_Check(pDX, IDC_CHECK_DROP_CONN, m_bDropConn);
	DDX_Check(pDX, IDC_CHECK_EDIT_EVENTDB, m_bEditEventDB);
	DDX_Check(pDX, IDC_CHECK_MANAGE_USERS, m_bManageUsers);
	DDX_Check(pDX, IDC_CHECK_VIEW_EVENTDB, m_bViewEventDB);
	DDX_Text(pDX, IDC_EDIT_DESCRIPTION, m_strDescription);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strName);
	DDX_Check(pDX, IDC_CHECK_MANAGE_ACTIONS, m_bManageActions);
	DDX_Check(pDX, IDC_CHECK_MANAGE_EPP, m_bManageEPP);
	DDX_Check(pDX, IDC_CHECK_MANAGE_CONFIG, m_bManageConfig);
	DDX_Check(pDX, IDC_CHECK_SNMP_TRAPS, m_bConfigureTraps);
	DDX_Check(pDX, IDC_CHECK_MANAGE_PKG, m_bManagePkg);
	DDX_Check(pDX, IDC_CHECK_DELETE_ALARMS, m_bDeleteAlarms);
	DDX_Check(pDX, IDC_CHECK_MANAGE_AGENT_CFG, m_bManageAgentCfg);
	DDX_Check(pDX, IDC_CHECK_MANAGE_SCRIPTS, m_bManageScripts);
	DDX_Check(pDX, IDC_CHECK_MANAGE_TOOLS, m_bManageTools);
	DDX_Check(pDX, IDC_CHECK_VIEW_TRAP_LOG, m_bViewTrapLog);
	DDX_Check(pDX, IDC_CHECK_ACCESS_FILES, m_bAccessFiles);
	DDX_Check(pDX, IDC_CHECK_REGISTER_AGENTS, m_bRegisterAgents);
	DDX_Check(pDX, IDC_CHECK_MANAGE_MAPS, m_bManageMaps);
	DDX_Check(pDX, IDC_CHECK_MANAGE_SITUATIONS, m_bManageSituations);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGroupPropDlg, CDialog)
	//{{AFX_MSG_MAP(CGroupPropDlg)
	ON_BN_CLICKED(IDC_BUTTON_ADD, OnButtonAdd)
	ON_BN_CLICKED(IDC_BUTTON_DELETE, OnButtonDelete)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGroupPropDlg message handlers


//
// WM_INITDIALOG message handler
//

BOOL CGroupPropDlg::OnInitDialog() 
{
   NXC_USER *pUser;
   CImageList *pImageList;
   RECT rect;
   DWORD i;
   int iItem;

	CDialog::OnInitDialog();
	
   // Disable controls not needed for EVERYONE group
   if (m_pGroup->dwId == GROUP_EVERYONE)
   {
      GetDlgItem(IDC_CHECK_DISABLED)->EnableWindow(FALSE);
      GetDlgItem(IDC_BUTTON_ADD)->EnableWindow(FALSE);
      GetDlgItem(IDC_BUTTON_DELETE)->EnableWindow(FALSE);
      GetDlgItem(IDC_LIST_MEMBERS)->EnableWindow(FALSE);
   }

   // Setup our own copy of group members
   m_dwNumMembers = m_pGroup->dwNumMembers;
   m_pdwMembers = (DWORD *)malloc(sizeof(DWORD) * m_dwNumMembers);
   memcpy(m_pdwMembers, m_pGroup->pdwMemberList, sizeof(DWORD) * m_dwNumMembers);

   // Setup list view
   m_wndListCtrl.GetClientRect(&rect);
   m_wndListCtrl.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 
                              rect.right - GetSystemMetrics(SM_CXVSCROLL) - 4);
	m_wndListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_TRACKSELECT | LVS_EX_UNDERLINEHOT);
   m_wndListCtrl.SetHoverTime(0x7FFFFFFF);

   // Create image list
   pImageList = new CImageList;
   pImageList->Create(16, 16, ILC_COLOR8 | ILC_MASK, 8, 8);
   pImageList->Add(AfxGetApp()->LoadIcon(IDI_USER));
   pImageList->Add(AfxGetApp()->LoadIcon(IDI_USER_GROUP));
   pImageList->Add(AfxGetApp()->LoadIcon(IDI_EVERYONE));
   m_wndListCtrl.SetImageList(pImageList, LVSIL_SMALL);

   // Fill in list control
   for(i = 0; i < m_dwNumMembers; i++)
   {
      pUser = NXCFindUserById(g_hSession, m_pdwMembers[i]);
      if (pUser != NULL)
      {
         iItem = m_wndListCtrl.InsertItem(0x7FFFFFFF, pUser->szName,
                                             (pUser->dwId == GROUP_EVERYONE) ? 2 :
                                                ((pUser->dwId & GROUP_FLAG) ? 1 : 0));
         if (iItem != -1)
            m_wndListCtrl.SetItemData(iItem, m_pdwMembers[i]);
      }
   }
	
	return TRUE;
}


//
// WM_COMMAND::IDC_BUTTON_ADD message handler
//

void CGroupPropDlg::OnButtonAdd() 
{
   CUserSelectDlg wndSelectDlg;

   wndSelectDlg.m_bOnlyUsers = TRUE;      // Don't show groups in selection dialog
   if (wndSelectDlg.DoModal() == IDOK)
   {
      DWORD i, j;
      int iItem = -1;

      for(j = 0; j < wndSelectDlg.m_dwNumUsers; j++)
      {
         // Check if we have this user in member list already
         for(i = 0; i < m_dwNumMembers; i++)
            if (m_pdwMembers[i] == wndSelectDlg.m_pdwUserList[j])
            {
               LVFINDINFO lvfi;

               // Find appropriate item in list
               lvfi.flags = LVFI_PARAM;
               lvfi.lParam = m_pdwMembers[i];
               iItem = m_wndListCtrl.FindItem(&lvfi);
               break;
            }

         if (i == m_dwNumMembers)
         {
            NXC_USER *pUser;

            // Create new entry in member list
            m_dwNumMembers++;
            m_pdwMembers = (DWORD *)realloc(m_pdwMembers, sizeof(DWORD) * m_dwNumMembers);
            m_pdwMembers[i] = wndSelectDlg.m_pdwUserList[j];

            // Add new line to user list
            pUser = NXCFindUserById(g_hSession, wndSelectDlg.m_pdwUserList[j]);
            if (pUser != NULL)
            {
               iItem = m_wndListCtrl.InsertItem(0x7FFFFFFF, pUser->szName,
                                                (pUser->dwId == GROUP_EVERYONE) ? 2 :
                                                   ((pUser->dwId & GROUP_FLAG) ? 1 : 0));
               m_wndListCtrl.SetItemData(iItem, wndSelectDlg.m_pdwUserList[j]);
            }
         }
      }

      // Select new item
      if (iItem != -1)
      {
         int iOldItem;

         iOldItem = m_wndListCtrl.GetNextItem(-1, LVNI_SELECTED);
         m_wndListCtrl.SetItemState(iItem, 0, LVIS_SELECTED | LVIS_FOCUSED);
         m_wndListCtrl.EnsureVisible(iItem, FALSE);
         m_wndListCtrl.SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, 
                                    LVIS_SELECTED | LVIS_FOCUSED);
      }
   }
}


//
// WM_COMMAND::IDC_BUTTON_DELETE message handler
//

void CGroupPropDlg::OnButtonDelete() 
{
   int nItem;
   DWORD i, dwUserId;

   nItem = m_wndListCtrl.GetNextItem(-1, LVIS_SELECTED);
   while(nItem != -1)
   {
      dwUserId = (DWORD)m_wndListCtrl.GetItemData(nItem);
      for(i = 0; i < m_dwNumMembers; i++)
      {
         if (m_pdwMembers[i] == dwUserId)
         {
            m_dwNumMembers--;
            memmove(&m_pdwMembers[i], &m_pdwMembers[i + 1], sizeof(DWORD) * (m_dwNumMembers - i));
            break;
         }
      }
      m_wndListCtrl.DeleteItem(nItem);
      nItem = m_wndListCtrl.GetNextItem(-1, LVIS_SELECTED);
   }
}