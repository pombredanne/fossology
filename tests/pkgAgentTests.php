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
* \brief Include file that describes the package agent functional tests
* to run
*
* @version "$Id: pkgAgentTests.php 4243 2011-05-21 01:07:20Z rrando $"
*
* Created on May 19, 2011 by Mark Donohoe
*/

// List of tests to run, BasicSetup must be run 1st as the rest of the tests
// depend on it.  Add your test at the end.
$ptests = array(
  'cliPkgagentTest.php',
  'UIPkgagentTest.php',
  );

$suiteName = 'Core Functional Tests';
// Test path is relative to ....fossology/tests/
$pTestPath = 'pkgagent';

$pkgAgentTests = array(
  'suiteName' => $suiteName,
  'testPath'  => $pTestPath,
  'tests' => $ptests,
);
?>