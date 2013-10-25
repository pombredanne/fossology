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
 * Describe your PHP program or function here
 *
 * @version "$Id: loginTest.php 4312 2011-06-01 20:00:39Z rrando $"
 *
 * Created on Jul 21, 2008
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
global $USER;
global $PASSWORD;

class TestRepoLogin extends fossologyTestCase{

  function testLogin(){

    global $URL;
    //print "login test starting\n";
    $browser = & new SimpleBrowser();
    $this->setBrowser($browser);
    $this->Login();
    $page = $this->mybrowser->getContent();
    //print "************LOGIN: Page after Login is:************\n";
    //$this->dump($page);
    preg_match('/FOSSology/', $page, $matches);
    //$this->dump($matches);
    $this->assertTrue($matches);
  }
}

?>