/**
 * 
 */
package org.netxms.nxmc.objectbrowser;

import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerComparator;
import org.netxms.client.NXCObject;

/**
 * @author Victor
 *
 */
public class ObjectTreeComparator extends ViewerComparator
{
	// Categories
	private static final int CATEGORY_CONTAINER = 10;
	private static final int CATEGORY_OTHER = 255;

	/* (non-Javadoc)
	 * @see org.eclipse.jface.viewers.ViewerComparator#category(java.lang.Object)
	 */
	@Override
	public int category(Object element)
	{
		if (((NXCObject)element).getObjectId() < 10)
			return (int)((NXCObject)element).getObjectId();
		if (((NXCObject)element).getObjectClass() == NXCObject.OBJECT_CONTAINER)
			return CATEGORY_CONTAINER;
		return CATEGORY_OTHER;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.viewers.ViewerComparator#compare(org.eclipse.jface.viewers.Viewer, java.lang.Object, java.lang.Object)
	 */
	@Override
	public int compare(Viewer viewer, Object e1, Object e2)
	{
		// TODO Auto-generated method stub
		return super.compare(viewer, e1, e2);
	}
}
