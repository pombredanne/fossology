<?php
/***********************************************************
 Copyright (C) 2011 Hewlett-Packard Development Company, L.P.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ***********************************************************/

/**
 * Is the folder edit properties menu available?
 *
 * @version "$Id: OrgUploadsMenuDelete-Test.php 4312 2011-06-01 20:00:39Z rrando $"
 *
 * Created on Jul 31, 2008
 */

$where = dirname(__FILE__);
if(preg_match('!/home/jenkins.*?tests.*!', $where, $matches))
{
  //echo "running from jenkins....fossology/tests\n";
  require_once('../../tests/fossologyTestCase.php');
  require_once ('../../tests/TestEnvironment.php');
}
else
{
  //echo "using requires for running outside of jenkins\n";
  require_once('../../../tests/fossologyTestCase.php');
  require_once ('../../../tests/TestEnvironment.php');
}

global $URL;

class UploadsDeleteMenuTest extends fossologyTestCase
{

  function testUploadsDeleteMenu()
  {
    global $URL;
    print "starting UploadsDeleteMenuTest\n";
    $this->Login();
    /* we get the home page to get rid of the user logged in page */
    $loggedIn = $this->mybrowser->get($URL);
    $this->assertTrue($this->myassertText($loggedIn, '/Organize/'));
    $this->assertTrue($this->myassertText($loggedIn, '/Uploads/'));
    $this->assertTrue($this->myassertText($loggedIn, '/Delete Uploaded File/'));
    $this->assertTrue($this->myassertText($loggedIn, '/Edit Properties/'));
    $this->assertTrue($this->myassertText($loggedIn, '/Move/'));
    /* ok, this proves the text is on the page, let's see if we can
     * get to the delete page.
     */
    $page = $this->mybrowser->get("$URL?mod=admin_upload_delete");
    $this->assertTrue($this->myassertText($page, '/Delete Uploaded File/'));
    $this->assertTrue($this->myassertText($page, '/THERE IS NO UNDELETE/'));
  }
}
?>
