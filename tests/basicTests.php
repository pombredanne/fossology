<?php
/*
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
 */

/**
* \brief Include file that describes the basic functional tests to run
*
* @version "$Id: basicTests.php 4242 2011-05-20 21:54:25Z rrando $"
*
* Created on May 19, 2011 by Mark Donohoe
*/

// List of tests to run, BasicSetup must be run 1st as the rest of the tests
// depend on it.  Add your test at the end.
$btests = array(
  'BasicSetup.php',
  'UploadFileTest.php',
  'UploadUrlTest.php',
  'UploadSrvArchiveTest.php',
  'uploadSrvDirTest.php',
  'uploadSrvFileTest.php',
  'CreateFolderTest.php',
  'DeleteFolderTest.php',
  'editFolderTest.php',
  'editFolderNameOnlyTest.php',
  'editFolderDescriptionOnlyTest.php',
  'moveFolderTest.php',
  'DupFolderTest.php',
  'DupUploadTest.php',
  'createFldrDeleteIt.php',
);

$suiteName = 'Core Functional Tests';
// Test path is relative to ....fossology/tests/
$basicTestPath = '../ui/tests/BasicTests';

$basicTests = array(
  'suiteName' => $suiteName,
  'testPath'  => $basicTestPath,
  'tests' => $btests,
);
?>