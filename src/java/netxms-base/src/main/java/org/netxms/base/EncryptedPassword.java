/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2013 Victor Kirhenshtein
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
package org.netxms.base;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import sun.misc.BASE64Decoder;
import sun.misc.BASE64Encoder;

/**
 * Utility class to encrypt and decrypt passwords in same way as nxcencpasswd do.
 * Please note that this functionality does not provide any protection for the passwords -
 * it's only allows to store passwords in configuration files in a not-immediately-readable form,
 * as required by some security standards.
 */
public class EncryptedPassword
{
   /**
    * Encrypt password for given login.
    * 
    * @param login
    * @param password
    * @return
    * @throws NoSuchAlgorithmException
    * @throws UnsupportedEncodingException
    */
   public static String encrypt(String login, String password) throws NoSuchAlgorithmException, UnsupportedEncodingException
   {
      byte[] key = MessageDigest.getInstance("MD5").digest(login.getBytes("UTF-8"));
      byte[] encrypted = IceCrypto.encrypt(Arrays.copyOf(password.getBytes("UTF-8"), 32), key);
      return new BASE64Encoder().encode(encrypted);
   }
   
   /**
    * @param login
    * @param password
    * @return
    * @throws NoSuchAlgorithmException
    * @throws IOException
    */
   public static String decrypt(String login, String password) throws NoSuchAlgorithmException, IOException
   {
      byte[] key = MessageDigest.getInstance("MD5").digest(login.getBytes("UTF-8"));
      byte[] encrypted = new BASE64Decoder().decodeBuffer(password);
      byte[] plainText = IceCrypto.decrypt(encrypted, key);
      int i;
      for(i = 0; i < plainText.length; i++)
         if (plainText[i] == 0)
            break;
      return new String(Arrays.copyOf(plainText, i), "UTF-8");
   }
}