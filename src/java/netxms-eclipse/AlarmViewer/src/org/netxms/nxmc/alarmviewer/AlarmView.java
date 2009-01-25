/**
 * 
 */
package org.netxms.nxmc.alarmviewer;

import java.util.HashMap;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.action.GroupMarker;
import org.eclipse.jface.action.IMenuListener;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.part.ViewPart;
import org.eclipse.ui.progress.IWorkbenchSiteProgressService;
import org.eclipse.ui.progress.UIJob;
import org.netxms.client.NXCAlarm;
import org.netxms.client.NXCException;
import org.netxms.client.NXCListener;
import org.netxms.client.NXCNotification;
import org.netxms.client.NXCSession;
import org.netxms.nxmc.core.extensionproviders.NXMCSharedData;

/**
 * @author victor
 *
 */
public class AlarmView extends Composite
{
	public static final String JOB_FAMILY = "AlarmViewJob";
	
	// Columns
	public static final int COLUMN_SEVERITY = 0;
	public static final int COLUMN_STATE = 1;
	public static final int COLUMN_SOURCE = 2;
	public static final int COLUMN_MESSAGE = 3;
	public static final int COLUMN_COUNT = 4;
	public static final int COLUMN_CREATED = 5;
	public static final int COLUMN_LASTCHANGE = 6;
	
	private final ViewPart viewPart;
	private NXCSession session;
	private TableViewer alarmViewer;
	private AlarmListFilter alarmFilter;
	private HashMap<Long, NXCAlarm> alarmList;
	
	public AlarmView(ViewPart viewPart, Composite parent, int style)
	{
		super(parent, style);
		session = NXMCSharedData.getSession();
		this.viewPart = viewPart;		
		
		// Setup table columns
		final String[] names = { "Severity", "State", "Source", "Message", "Count", "Created", "Last Change" };
		final int[] widths = { 100, 100, 150, 300, 70, 90, 90 };
		final Table table = new Table(this, SWT.MULTI | SWT.VIRTUAL | SWT.FULL_SELECTION);
		final TableColumn[] tc = new TableColumn[names.length];
		alarmViewer = new TableViewer(table);
		TableSortingListener sortingListener = new TableSortingListener(alarmViewer);
		for(int i = 0; i < names.length; i++)
		{
			tc[i] = new TableColumn(table, SWT.LEFT);
			tc[i].setText(names[i]);
			tc[i].setWidth(widths[i]);
			tc[i].setData("ID", new Integer(i));
			tc[i].addSelectionListener(sortingListener);
		}
		table.setLinesVisible(true);
		table.setHeaderVisible(true);
		
		alarmViewer.setLabelProvider(new AlarmListLabelProvider());
		alarmViewer.setContentProvider(new ArrayContentProvider());
		alarmViewer.getTable().setSortColumn(tc[0]);
		alarmViewer.getTable().setSortDirection(SWT.DOWN);
//		alarmViewer.setComparator(new AlarmComparator());
		alarmFilter = new AlarmListFilter();
		alarmViewer.addFilter(alarmFilter);
		
		createPopupMenu();

		final Composite alarmView = this;
		addListener(SWT.Resize, new Listener() {
			public void handleEvent(Event e)
			{
				alarmViewer.getControl().setBounds(alarmView.getClientArea());
			}
		});

		// Request alarm list from server
		Job job = new Job("Synchronize alarm list")
		{
			@Override
			protected IStatus run(IProgressMonitor monitor)
			{
				IStatus status;
				
				try
				{
					alarmList = session.getAlarms(false);
					status = Status.OK_STATUS;

					new UIJob("Initialize alarm viewer") {
						@Override
						public IStatus runInUIThread(IProgressMonitor monitor)
						{
							synchronized(alarmList)
							{
								alarmViewer.setInput(alarmList.values());
							}
							return Status.OK_STATUS;
						}
					}.schedule();
				}
				catch(Exception e)
				{
					status = new Status(Status.ERROR, Activator.PLUGIN_ID, 
	                    (e instanceof NXCException) ? ((NXCException)e).getErrorCode() : 0,
	                    "Cannot synchronize alarm list: " + e.getMessage(), e);
				}
				return status;
			}

			/* (non-Javadoc)
			 * @see org.eclipse.core.runtime.jobs.Job#belongsTo(java.lang.Object)
			 */
			@Override
			public boolean belongsTo(Object family)
			{
				return family == AlarmView.JOB_FAMILY;
			}
		};
		IWorkbenchSiteProgressService siteService =
	      (IWorkbenchSiteProgressService)viewPart.getSite().getAdapter(IWorkbenchSiteProgressService.class);
		siteService.schedule(job, 0, true);
		
		// Add client library listener
		session.addListener(new NXCListener() {
			@Override
			public void notificationHandler(NXCNotification n)
			{
				switch(n.getCode())
				{
					case NXCNotification.NEW_ALARM:
					case NXCNotification.ALARM_CHANGED:
						synchronized(alarmList)
						{
							alarmList.put(((NXCAlarm)n.getObject()).getId(), (NXCAlarm)n.getObject());
						}
						scheduleAlarmViewerUpdate();
						break;
					case NXCNotification.ALARM_TERMINATED:
					case NXCNotification.ALARM_DELETED:
						synchronized(alarmList)
						{
							alarmList.remove(((NXCAlarm)n.getObject()).getId());
						}
						scheduleAlarmViewerUpdate();
						break;
					default:
						break;
				}
			}
		});
	}
	
	
	/**
	 * Schedule alarm viewer update
	 */
	private void scheduleAlarmViewerUpdate()
	{
		new UIJob("Update alarm list") {
			@Override
			public IStatus runInUIThread(IProgressMonitor monitor)
			{
				synchronized(alarmList)
				{
					alarmViewer.refresh();
				}
				return Status.OK_STATUS;
			}
		}.schedule();
	}
	

	/**
	 * Create pop-up menu for object browser
	 */
	
	private void createPopupMenu()
	{
		// Create menu manager.
		MenuManager menuMgr = new MenuManager();
		menuMgr.setRemoveAllWhenShown(true);
		menuMgr.addMenuListener(new IMenuListener()
		{
			public void menuAboutToShow(IMenuManager mgr)
			{
				fillContextMenu(mgr);
			}
		});

		// Create menu.
		Menu menu = menuMgr.createContextMenu(alarmViewer.getControl());
		alarmViewer.getControl().setMenu(menu);

		// Register menu for extension.
		if (viewPart != null)
			viewPart.getSite().registerContextMenu(menuMgr, alarmViewer);
	}
	

	/**
	 * Fill context menu
	 * @param mgr Menu manager
	 */
	protected void fillContextMenu(IMenuManager mgr)
	{
		mgr.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
	}

	
	/**
	 * Change root object for alarm list
	 * 
	 * @param objectId ID of new root object
	 */
	public void setRootObject(long objectId)
	{
		alarmFilter.setRootObject(objectId);
		synchronized(alarmList)
		{
			alarmViewer.refresh();
		}
	}
}
