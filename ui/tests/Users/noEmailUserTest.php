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
 * Add a user using the UI, with no email notification,
 * verify it is set correctly in their session.
 *
 * @version "$Id: noEmailUserTest.php 4310 2011-06-01 18:47:18Z rrando $"
 *
 * Created on March 17, 2009
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

require_once (TESTROOT . '/testClasses/db.php');

global $URL;

class noEmailUserTest extends fossologyTestCase {

  public $mybrowser;

  function setUP() {
    global $URL;
    $this->Login();
  }
  function testNoEmailUser() {
    global $URL;

    /* Check that the user exists */
    $loggedIn = $this->mybrowser->get($URL);
    $this->assertTrue($this->myassertText($loggedIn, '/Admin/'));
    $this->assertTrue($this->myassertText($loggedIn, '/Users/'));
      $page = $this->mybrowser->get("$URL?mod=user_add");
    $this->assertTrue($this->myassertText($page, '/Add A User/'));
    $this->assertTrue($this->myassertText($page, '/To create a new user,/'));
    $result = $this->addUser('UserNoEmail', 'No email notification user',
                                 'fosstester', 1, 1, 'noetest', NULL);
    if (!is_null($result)) {
      /*
       * The test is just creating the user so we can verify that email
       * notification was not turned on.  So, it's OK to have the user already
       * there, for this test it's not an error.
       */
      if ($result != "User already exists.  Not added") {
        $this->fail("Did not add user UserwEmail result was:\n$result\n");
      }
    }

    /*
     * Verify, check the db for this user to ensure email_notify is NOT set.
     */
    $dlink = new db('host=localhost dbname=fossology user=fossy password=fossy;');
    print "Verifying User email notification setting\n";
    $Sql = "SELECT user_name, email_notify FROM users WHERE user_name='UserNoEmail';";
    $User = $dlink->dbQuery($Sql);
    //print "DB: User(SQL results) are:\n";print_r($User) . "\n";
    if((int)$User[0]['email_notify'] == 0) {
      $this->pass();
    }
    else {
      $this->fail("Fail! User UserNoEmail email_notify is not NULL\n");
      printf("in octal the entry for email_notify is:%o\n",$User[0]['email_notify']);
    }
  } //testNoEmailUser
}
?>
