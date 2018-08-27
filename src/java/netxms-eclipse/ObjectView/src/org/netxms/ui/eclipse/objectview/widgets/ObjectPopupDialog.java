/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2017 Raden Solutions
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
package org.netxms.ui.eclipse.objectview.widgets;

import java.util.List;
import org.eclipse.jface.dialogs.PopupDialog;
import org.eclipse.jface.layout.GridDataFactory;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.CLabel;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.FontData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.model.WorkbenchLabelProvider;
import org.netxms.client.datacollection.DciValue;
import org.netxms.client.objects.AbstractNode;
import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.DataCollectionTarget;
import org.netxms.ui.eclipse.console.resources.StatusDisplayInfo;

/**
 * Pop-up dialog containing summary information for given object
 */
public class ObjectPopupDialog extends PopupDialog
{
   private AbstractObject object;
   private Point location;
   private CLabel statusLabel = null;
   private Font boldFont = null;
   private WorkbenchLabelProvider labelProvider;
   
   /**
    * @param parent parent shell
    * @param object object to show information from
    * @param location dialog location (in display coordinates)
    */
   public ObjectPopupDialog(Shell parent, AbstractObject object, Point location)
   {
      super(parent, HOVER_SHELLSTYLE, true, false, false, false, false, object.getObjectName(), null);
      this.object = object;
      this.location = (location != null) ? location : Display.getCurrent().getCursorLocation();
      labelProvider = new WorkbenchLabelProvider();
   }

   /* (non-Javadoc)
    * @see org.eclipse.jface.dialogs.PopupDialog#configureShell(org.eclipse.swt.widgets.Shell)
    */
   @Override
   protected void configureShell(Shell shell)
   {
      super.configureShell(shell);
      shell.addDisposeListener(new DisposeListener() {
         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            if (boldFont != null)
               boldFont.dispose();
            labelProvider.dispose();
         }
      });
   }

   /* (non-Javadoc)
    * @see org.eclipse.jface.dialogs.PopupDialog#getInitialLocation(org.eclipse.swt.graphics.Point)
    */
   @Override
   protected Point getInitialLocation(Point initialSize)
   {
      return location;
   }

   /* (non-Javadoc)
    * @see org.eclipse.jface.dialogs.PopupDialog#createTitleControl(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createTitleControl(Composite parent)
   {
      CLabel title = new CLabel(parent, SWT.NONE);
      GridDataFactory.fillDefaults().align(SWT.FILL, SWT.CENTER).grab(true, false).span(2, 1).applyTo(title);
      title.setImage(labelProvider.getImage(object));
      title.setText(object.getObjectName());
      
      FontData fd = title.getFont().getFontData()[0];
      fd.setStyle(SWT.BOLD);
      boldFont = new Font(title.getDisplay(), fd);
      title.setFont(boldFont);

      return title;
   }
   
   /* (non-Javadoc)
    * @see org.eclipse.jface.dialogs.PopupDialog#createDialogArea(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createDialogArea(Composite parent)
   {
      Composite dialogArea = (Composite)super.createDialogArea(parent);

      statusLabel = new CLabel(dialogArea, SWT.NONE);
      statusLabel.setText(StatusDisplayInfo.getStatusText(object.getStatus()).toUpperCase());
      statusLabel.setForeground(StatusDisplayInfo.getStatusColor(object.getStatus()));
      statusLabel.setFont(boldFont);
      
      StringBuilder sb = new StringBuilder();
      if (object instanceof AbstractNode)
      {
         appendText(sb, ((AbstractNode)object).getPrimaryIP().getHostAddress());
         appendText(sb, ((AbstractNode)object).getPlatformName());
         String sd = ((AbstractNode)object).getSystemDescription();
         if (sd.length() > 127)
            sd = sd.substring(0, 127) + "..."; //$NON-NLS-1$
         appendText(sb, sd);
         appendText(sb, ((AbstractNode)object).getSnmpSysName());
         appendText(sb, ((AbstractNode)object).getSnmpSysContact());
      }

      if (sb.length() > 0)
      {
         createSeparator(dialogArea);
         
         if (sb.charAt(sb.length() - 1) == '\n')
            sb.deleteCharAt(sb.length() - 1);
         CLabel infoText = new CLabel(dialogArea, SWT.MULTI);
         infoText.setText(sb.toString());
      }
      
      if (object instanceof DataCollectionTarget)
      {
         List<DciValue> values = ((DataCollectionTarget)object).getTooltipDciData();
         if (!values.isEmpty())
         {
            createSeparator(dialogArea);
            
            Composite group = new Composite(dialogArea, SWT.NONE);
            GridLayout layout = new GridLayout();
            layout.marginHeight = 0;
            layout.marginWidth = 0;
            layout.numColumns = 2;
            group.setLayout(layout);

            StringBuilder leftColumn = new StringBuilder();
            StringBuilder rightColumn = new StringBuilder();
            for(DciValue v : values)
            {
               if (leftColumn.length() > 0)
               {
                  leftColumn.append('\n');
                  rightColumn.append('\n');
               }
               leftColumn.append(v.getDescription());
               rightColumn.append(v.getValue());
            }
            
            new CLabel(group, SWT.MULTI).setText(leftColumn.toString());
            new CLabel(group, SWT.MULTI).setText(rightColumn.toString());
         }
      }
      
      if (!object.getComments().isEmpty())
      {
         createSeparator(dialogArea);
         new CLabel(dialogArea, SWT.MULTI).setText(object.getComments());
      }
      
      return dialogArea;
   }
   
   /* (non-Javadoc)
    * @see org.eclipse.jface.dialogs.PopupDialog#getForegroundColorExclusions()
    */
   @Override
   protected List<Control> getForegroundColorExclusions()
   {
      List<Control> e = super.getForegroundColorExclusions();
      e.add(statusLabel);
      return e;
   }

   /**
    * Create separator line
    * 
    * @param parent
    */
   private static void createSeparator(Composite parent) 
   {
      Label separator = new Label(parent, SWT.SEPARATOR | SWT.HORIZONTAL | SWT.LINE_DOT);
      GridDataFactory.fillDefaults().align(SWT.FILL, SWT.CENTER).grab(true, false).applyTo(separator);
   }

   /**
    * @param sb
    * @param text
    */
   private static void appendText(StringBuilder sb, String text)
   {
      if ((text == null) || text.isEmpty())
         return;
      sb.append(text);
      sb.append('\n');
   }
}
