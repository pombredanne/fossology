<?php
/***********************************************************
 Copyright (C) 2008 Hewlett-Packard Development Company, L.P.

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
/*************************************************
 Restrict usage: Every PHP file should have this
 at the very beginning.
 This prevents hacking attempts.
 *************************************************/

/**
 * agent-nomos-once
 * \brief Run an analysis for a single file, do not store results in the DB.
 *
 * @version "$Id: agent-nomos-once.php 4092 2011-04-16 00:12:10Z rrando $"
 */
global $GlobalReady;
if (!isset($GlobalReady)) {
  exit;
}

define("TITLE_agent_nomos_once", _("One-Shot License Analysis"));

class agent_nomos_once extends FO_Plugin {

  public $Name = "agent_nomos_once";
  public $Title = TITLE_agent_nomos_once;
  public $Version = "1.0";
  /* note: no menulist needed, it's insterted in the code below */
  //public $Dependency = array();
  public $NoHTML = 0;  // always print text output for now
  /** For anyone to access, without login, use: **/
  public $DBaccess   = PLUGIN_DB_NONE;
  public $LoginFlag  = 0;

  /** To require login access, use: **/
//  public $DBaccess = PLUGIN_DB_ANALYZE;
//  public $LoginFlag = 1;

  /**
   * AnalyzFile(): Analyze one uploaded file.
   *
   * @param string $FilePath the filepath to the file to analyze.
   * @return string $V, html to display the results.
   *
   */
  function AnalyzeFile($FilePath) {
  	
    global $Plugins;
    global $AGENTDIR;
    
    $licenses = array();

    $licenseResult = "";
    /* move the temp file */
    $licenseResult = exec("$AGENTDIR/nomos $FilePath",$out,$rtn);
    $licenses = explode(' ',$out[0]);
    $last = end($licenses);
    return ($last);
    
  } // AnalyzeFile()

  /*********************************************
   RegisterMenus(): Change the type of output
   based on user-supplied parameters.
   Returns 1 on success.
   *********************************************/
  function RegisterMenus() {
    if ($this->State != PLUGIN_STATE_READY) {
      return (0);
    } // don't run
    $ShowHeader = GetParm('showheader', PARM_INTEGER);
    if (empty($ShowHeader)) {
      $ShowHeader = 0;
    }
    if (GetParm("mod", PARM_STRING) == $this->Name) {
      $ThisMod = 1;
    }
    else {
      $ThisMod = 0;
    }
    /*
     * This if stmt is true only for wget.
     * For wget, populate the $_FILES array, just like the UI post would do.
     * Sets the unlink_flag if there is a temp file.
     */
    if ($ThisMod && empty($_POST['showheader']) && ($_SERVER['REQUEST_METHOD'] == "POST"))
    {
      $Fin = fopen("php://input", "r");
      $Ftmp = tempnam(NULL, "fosslic-alo-");
      $Fout = fopen($Ftmp, "w");
      while (!feof($Fin)) {
        $Line = fgets($Fin);
        fwrite($Fout, $Line);
      }
      fclose($Fin);
      fclose($Fout);

      /* Populate _FILES from wget so the processing logic only has to look in one
       * place wether the data came from wget or the UI
       */
      if (filesize($Ftmp) > 0)
      {
        $_FILES['licfile']['tmp_name'] = $Ftmp;
        $_FILES['licfile']['size'] = filesize($Ftmp);
        $_FILES['licfile']['unlink_flag'] = 1;
        $this->NoHTML = 1;
      }
      else
      {
        unlink($Ftmp);
        /* If there is no input data, then something is wrong.
         * For example the php POST limit is too low and prevented
         * the data from coming through.  Or there was an apache redirect,
         * which removes the POST data.
         */
        $text = _("FATAL: your file did not get passed throught.  Make sure this page wasn't a result of a web server redirect, or that it didn't exceed your php POST limit.");
        echo $text;
      }
    }

    /* Only register with the menu system if the user is logged in. */
    if (!empty($_SESSION['User'])) {
      if (@$_SESSION['UserLevel'] >= PLUGIN_DB_ANALYZE) {
        menu_insert("Main::Upload::One-Shot Analysis", $this->MenuOrder, $this->Name, $this->MenuTarget);
      }
      // Debugging changes to license analysis
      if (@$_SESSION['UserLevel'] >= PLUGIN_DB_DEBUG) {
        $URI = $this->Name . Traceback_parm_keep(array(
          "format",
          "item"
          ));
          menu_insert("View::[BREAK]", 100);
$text = _("One-shot License, real-time license analysis");
          menu_insert("View::One-Shot License", 101, $URI, $text);
          menu_insert("View-Meta::[BREAK]", 100);
$text = _("Nomos One-shot, real-time license analysis");
          menu_insert("View-Meta::One-Shot License", 101, $URI, $text);
      }
    }
  } // RegisterMenus()
  /*********************************************
  Output(): Generate the text for this plugin.
  *********************************************/
  function Output() {
    if ($this->State != PLUGIN_STATE_READY) {
      return;
    }
    global $DB;
    global $DATADIR;
    global $PROJECTSTATEDIR;

    /* Ignore php Notice is array keys don't exist */
    $errlev = error_reporting(E_ERROR | E_WARNING | E_PARSE);
    $tmp_name = $_FILES['licfile']['tmp_name'];
    error_reporting($errlev);

    /* For REST API:
       wget -qO - --post-file=myfile.c http://myserv.com/?mod=agent_nomos_once
     */
    if ($this->NoHTML && file_exists($tmp_name))
    {
       echo $this->AnalyzeFile($tmp_name);
       echo "\n";
       unlink($tmp_name);
       return;
    }

    $V = "";
    switch ($this->OutputType) {
      case "XML":
        break;
      case "HTML":
        /* Display instructions */
        $V.= _("This analyzer allows you to upload a single file for license analysis.\n");
        $V.= _("The limitations:\n");
        $V.= "<ul>\n";
        $V.= _("<li>The analysis is done in real-time. Large files may take a while." .
             " This method is not recommended for files larger than a few hundred kilobytes.\n");
$text = _("Files that contain files are");
$text1 = _("not");
$text2 = _("unpacked. If you upload a 'zip' or 'deb' file, then the binary file will be scanned for licenses and nothing will likely be found.");
        $V.= "<li>$text <b>$text1</b> $text2\n";
$text = _("Results are");
$text1 = _("not");
$text2 = _("stored. As soon as you get your results, your uploaded file is removed from the system. ");
        $V.= "<li>$text <b>$text1</b> $text2\n";
        $V.= "</ul>\n";
        /* Display the form */
        $V.= "<form enctype='multipart/form-data' method='post'>\n";
        $V.= "<ul>\n";
        $V.= _("<li>Select the file to upload:<br />\n");
        $V.= "<input name='licfile' size='60' type='file' /><br />\n";
        $V.= "</ul>\n";
        $V.= "<input type='hidden' name='showheader' value='1'>";
$text = _("Analyze");
        $V.= "<input type='submit' value='$text!'>\n";
        $V.= "</form>\n";


        if (file_exists($tmp_name)) {
$text = _("A one shot license analysis shows the following license(s) in file");
          $keep = "<strong>$text </strong><em>{$_FILES['licfile']['name']}:</em> ";
          $keep .= "<strong>" . $this->AnalyzeFile($tmp_name) . "</strong><br>";
          print displayMessage(NULL,$keep);
          $_FILES['licfile'] = NULL;
          print $V;

          if (!empty($_FILES['licfile']['unlink_flag'])) {
            unlink($tmp_name);
          }
          return;
        }

        $Item = GetParm('item', PARM_INTEGER); // may be null
        if (!empty($Item) && !empty($DB) && !empty($_FILES)) {
          /* Get the pfile info */
          $Results = $DB->Action("SELECT * FROM pfile
		        INNER JOIN uploadtree ON uploadtree_pk = $Item
		        AND pfile_pk = pfile_fk;");
          if (!empty($Results[0]['pfile_pk'])) {
            global $LIBEXECDIR;
            $Highlight = 1; /* processing a pfile? Always highlight. */
            $Repo = $Results[0]['pfile_sha1'] . "." . $Results[0]['pfile_md5'] . "." . $Results[0]['pfile_size'];
            $Repo = trim(shell_exec("$LIBEXECDIR/reppath files '$Repo'"));
            $tmp_name = $Repo;
            $text = _("A one shot license analysis shows the following license(s) in file");
            $keep = "<strong>$text </strong><em>{$_FILES['licfile']['name']}:</em> ";
            $keep .= "<strong>" . $this->AnalyzeFile($tmp_name) . "</strong><br>";
            print displayMessage(NULL, $keep);
            print $V;
            /* Do not unlink the or it will delete the repo file! */
            if (!empty($_FILES['licfile']['unlink_flag'])) {
              unlink($tmp_name);
            }
            return;
          }
        }
        break;
      case "Text":
        break;
      default:
        break;
    }
    if (!empty($_FILES['licfile']['unlink_flag'])) {
      unlink($_FILES['licfile']['tmp_name']);
    }
    if (!$this->OutputToStdout) {
      return ($V);
    }
    print ($V);
    return;
  }
};
$NewPlugin = new agent_nomos_once;
?>
