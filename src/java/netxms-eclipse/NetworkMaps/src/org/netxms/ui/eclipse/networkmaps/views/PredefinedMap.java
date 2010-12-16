/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2010 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.networkmaps.views;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.progress.UIJob;
import org.eclipse.zest.core.widgets.ZestStyles;
import org.netxms.client.NXCObjectModificationData;
import org.netxms.client.maps.NetworkMapLink;
import org.netxms.client.maps.elements.NetworkMapObject;
import org.netxms.client.objects.GenericObject;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.networkmaps.Activator;
import org.netxms.ui.eclipse.objectbrowser.dialogs.ObjectSelectionDialog;

/**
 * View for predefined map
 *
 */
public class PredefinedMap extends NetworkMap
{
	public static final String ID = "org.netxms.ui.eclipse.networkmaps.views.PredefinedMap";
	
	private org.netxms.client.objects.NetworkMap mapObject; 
	private Action actionAddObject;
	private Action actionLinkObjects;
	
	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#init(org.eclipse.ui.IViewSite)
	 */
	@Override
	public void init(IViewSite site) throws PartInitException
	{
		super.init(site);
		mapObject = (org.netxms.client.objects.NetworkMap)rootObject;
		setPartName(rootObject.getObjectName());
	}

	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#createPartControl(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	public void createPartControl(Composite parent)
	{
		super.createPartControl(parent);
		viewer.addSelectionChangedListener(new ISelectionChangedListener() {
			@Override
			public void selectionChanged(SelectionChangedEvent event)
			{
				actionLinkObjects.setEnabled(((IStructuredSelection)event.getSelection()).size() == 2);
			}
		});
	}

	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#buildMapPage()
	 */
	@Override
	protected void buildMapPage()
	{
		mapPage = ((org.netxms.client.objects.NetworkMap)rootObject).createMapPage();
	}

	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#createActions()
	 */
	@Override
	protected void createActions()
	{
		super.createActions();
		
		actionAddObject = new Action("&Add object...") {
			@Override
			public void run()
			{
				addObjectToMap();
			}
		};
		actionAddObject.setAccelerator(SWT.CTRL | 'A');
		
		actionLinkObjects = new Action("&Link selected objects") {
			@Override
			public void run()
			{
				linkSelectedObjects();
			}
		};
		actionLinkObjects.setAccelerator(SWT.CTRL | 'L');
		actionLinkObjects.setImageDescriptor(Activator.getImageDescriptor("icons/link_add.png"));
	}
	
	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#fillMapContextMenu(org.eclipse.jface.action.IMenuManager)
	 */
	@Override
	protected void fillMapContextMenu(IMenuManager manager)
	{
		manager.add(actionAddObject);
		manager.add(new Separator());
		super.fillMapContextMenu(manager);
	}

	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#fillObjectContextMenu(org.eclipse.jface.action.IMenuManager)
	 */
	@Override
	protected void fillObjectContextMenu(IMenuManager manager)
	{
		if (((IStructuredSelection)viewer.getSelection()).size() == 2)
		{
			manager.add(actionLinkObjects);
			manager.add(new Separator());
		}
		super.fillObjectContextMenu(manager);
	}

	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#fillLocalPullDown(org.eclipse.jface.action.IMenuManager)
	 */
	@Override
	protected void fillLocalPullDown(IMenuManager manager)
	{
		manager.add(actionAddObject);
		manager.add(actionLinkObjects);
		manager.add(new Separator());
		super.fillLocalPullDown(manager);
	}

	/* (non-Javadoc)
	 * @see org.netxms.ui.eclipse.networkmaps.views.NetworkMap#fillLocalToolBar(org.eclipse.jface.action.IToolBarManager)
	 */
	@Override
	protected void fillLocalToolBar(IToolBarManager manager)
	{
		manager.add(actionLinkObjects);
		manager.add(new Separator());
		super.fillLocalToolBar(manager);
	}

	/**
	 * Add object to map
	 */
	private void addObjectToMap()
	{
		ObjectSelectionDialog dlg = new ObjectSelectionDialog(getSite().getShell(), null, null);
		if (dlg.open() != Window.OK)
			return;
		
		int added = 0;
		for(GenericObject object : dlg.getSelectedObjects())
		{
			if (mapPage.findObjectElement(object.getObjectId()) == null)
			{
				mapPage.addElement(new NetworkMapObject(mapPage.createElementId(), object.getObjectId()));
				added++;
			}
		}
		
		if (added > 0)
			saveMap();
	}
	
	/**
	 * Link currently selected objects
	 */
	private void linkSelectedObjects()
	{
		IStructuredSelection selection = (IStructuredSelection)viewer.getSelection();
		if (selection.size() != 2)
			return;
		
		Object[] objects = selection.toArray();
		long id1 = ((NetworkMapObject)objects[0]).getId();
		long id2 = ((NetworkMapObject)objects[1]).getId();
		if (!mapPage.areObjectsConnected(id1, id1))
		{
			mapPage.addLink(new NetworkMapLink(NetworkMapLink.NORMAL, id1, id2));
			saveMap();
		}
	}
	
	/**
	 * Save map on server
	 */
	private void saveMap()
	{
		final NXCObjectModificationData md = new NXCObjectModificationData(rootObject.getObjectId());
		md.setMapContent(mapPage.getElements(), mapPage.getLinks());
		new ConsoleJob("Save map object " + rootObject.getObjectName(), this, Activator.PLUGIN_ID, Activator.PLUGIN_ID) {
			@Override
			protected void runInternal(IProgressMonitor monitor) throws Exception
			{
				session.modifyObject(md);
				new UIJob("Refresh map view") {
					@Override
					public IStatus runInUIThread(IProgressMonitor monitor)
					{
						viewer.setInput(mapPage);
						return Status.OK_STATUS;
					}
				};
			}

			@Override
			protected String getErrorMessage()
			{
				return "Cannot update map content on server";
			}
		}.start();
	}
}
