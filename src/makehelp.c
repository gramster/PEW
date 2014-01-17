#include <stdio.h>
#include "misc.h"
#include "help.h"

#define MAXHELP 128

char *HelpText[MAXHELP] = {
/* 0 FILEMENU	*/
    "The File Submenu contains commands pertaining to file "
    "operations, such as loading and storing files to and from "
    "disk, changing the name of a file, and so on.",

/* 1 EDITCMD	*/
    "The Edit command returns you to the editor.",

/* 2 OPTMENU	*/
    "The Options Submenu contains various items which can be "
    "altered to reconfigure the editor to suit your requirements.",

/* 3 EXECCMD	*/
    "The Execute command causes your compiled editor"
    " file to be submitted to the Estelle interpreter. If the"
    " compiled version of the program is out of date, the "
    "Estelle compiler will first be invoked to recompile the"
    " program.",

/* 4 ANLYZCMD	*/
    "The Analyze Submenu allows you to run several versions of "
    "a specification and tabulate the results. Each version to "
    "be executed differs from the others in the value assumed by"
    " a global constant. For example, you may wish to calculate "
    "the throughput and error rate of a protocol under different"
    " network reliabilities. In this case, the independent "
    "variable is reliability, while the dependents are throughput "
    "and error rate. You will use the options on this menu to "
    "specify the range of values to be used for reliability. "
    "You will also enter expressions for the dependent values. "
    "Finally you will enter an execution timelimit. The PEW will"
    " then repeatedly recompile and reexecute the specification "
    "for each value of the independent variable, and write a "
    "table of results to the log file.",

/* 5 COMPILE	*/
    "The Compile command will cause your current editor"
    " file to be submitted to the Estelle compiler.",

/* 6 MAINMENU	*/
    "The main menu bar has"
    " several submenus and commands (which have their own "
    "help). You can move around on the menu bar by pressing"
    " the left and right cursor keys, or by using the first"
    " letter of the menu option (X for execute), or by using"
    " the appropriate control key (eg CTRL-X for execute)."
    "~The control key method allows you to immediately go "
    "to a specific menu option from the editor.~To select a"
    " menu option you use Carriage Return; to leave a submenu"
    " you use ESC.~Within the submenus, you can again use the"
    " first letter of an option as a selection shortcut.",

/* 7 LOADFILE	*/
    "Loads a new file into the Editor",

/* 8 SAVEFILE	*/
    "Saves the current workspace to disk",

/* 9 RENAME	*/
    "Changes the save name of the current file in the Editor",

/* 10 NEWFILE	*/
    "Clears the Editor workspace to begin working on a new file",

/* 11 GOTOLINE	*/
    "Goes to a specific line of the current workfile",

/* 12 OSSHELL	*/
    "Exits temporarily to DOS. You may then run DOS commands"
    " at the DOS prompt. When you wish to return to the PEW"
    ", you type 'exit'. Use this command if you want to run"
    " a DOS command without quitting the PEW.",

/* 13 QUIT		*/
    "Exits the PEW and return to DOS",

/* 14 TABSIZE	*/
    "Changes the tab stop setting. This will affect only"
    " tabs entered from after the change is made. If you"
    " wish to change the tab settings on an entire file"
    ", first save the file, then change the tab setting,"
    " and then reload the file with the new tab setting.",

/* 15 BACKUP	*/
    "Toggles file backing up on and off. If backups are selected"
    ", then whenever you save a file, the previous contents are"
    " saved in a file with the same name but a .BAK extension.",

/* 16 FILLCHAR	*/
    "Switches whitespace fill character between tabs or spaces."
    " This is the character used to fill whitespace when you"
    " save a file. In the editor, all tabs are expanded to "
    "spaces. If you select space as the fill character, files "
    "will be saved exactly as they appear in the editor. If "
    "you select tabs, then wherever possible spaces will be "
    "recompressed to tabs upon saving a file.",

/* 17 WRAPMODE	*/
    "Toggles between wrap or scroll of long lines. When wrap is"
    " on, long lines are split up over several lines on the "
    "screen. If wrap is off, only as much of a line as can "
    "fit on a screen line is displayed.",

/* 18 LEFTSRCH	*/
    "Alters the set of characters matched by a left search. A"
    " left search command will search left for the first "
    "occurrence of a character from this string. It is useful"
    ", for example, for matching parentheses.",

/* 19 RIGHTSRCH	*/
    "Alters the set of characters matched by a right search."
    " See the help for left search for more details.",

/* 20 SRCHTEXT	*/
    "Sets the text which will be used by the Search command.",

/* 21 TRANSTXT	*/
    "Sets the text which will be used to replace matched text "
    "by the Translate command.",

/* 22 CASESENS	*/
    "Toggles search case sensitivity on/off.",

/* 23 CFGAUTO	*/
    "Toggles configuration save on exit on or off. If on, the"
    " configuration will automatically be saved when you quit"
    " the PEW.",

/* 24 CFGSAVE	*/
    "Saves the current Editor configuration. This consists of"
    " the current file name and position, the current key "
    "assignments, the current option settings, and the current"
    " keystroke macros.",

/* 25 FIXDCMDS	*/
    "The fixed commands are those few basic commands that "
    "cannot be reassigned to other keys. They include basic "
    "movement commands and the help command.",

/* 26 MOVECMDS	*/
    "The movement commands are those cursor movement commands"
    " that can be reassigned to other keys. The simplest "
    "movement commands can be found in the 'Fixed commands"
    " section. See also the Search commands.",

/* 27 BLKCMDS	*/
    "The block commands are those which allow you to define, "
    "delete and move blocks of text. The File menu on the "
    "main menu bar has commands for loading/saving blocks "
    "to disk.",

/* 28 SRCHTRANS	*/
    "The search/translate commands allow you to search for "
    "specific text, and to optionally replace matches with "
    "some other text.",

/* 29 MISC		*/
    "Various commands (for example, Undo).",

/* 30 CURSUP	*/
    "Moves the cursor up one line.",

/* 31 CURSDOWN	*/
    "Moves the cursor down one line.",

/* 32 CURSLEFT	*/
    "Moves the cursor left one character.",

/* 33 CURSRGHT	*/
    "Moves the cursor right one character.",

/* 34 HOME		*/
    "Moves the cursor to the start of the line.",

/* 35 END		*/
    "Moves the cursor to the end of the line.",

/* 36 PGUP		*/
    "Moves the cursor to the first line of the current screen, "
    "unless it is already there, in which case moves it up a "
    "whole screenful of text.",

/* 37 PGDN		*/
    "Moves the cursor to the last line of the current screen, "
    "unless it is already there, in which case moves it down a "
    "whole screenful of text.",

/* 38 DELETE	*/
    "Deletes the character under the cursor. If a block has "
    "been marked, the entire block is deleted and copied to "
    "the block scrap buffer, from where it can be pasted into "
    "the file. See the Block Commands help for more details.",

/* 39 HELP		*/
    "Provides help on the current item.",

/* 40 WORDLEFT	*/
    "Moves the cursor left to the start of the previous word.",

/* 41 WORDRGHT	*/
    "Moves the cursor right to the start of the next word.",

/* 42 TOF		*/
    "Moves the cursor to line 1, column 1 of the file.",

/* 43 EOF		*/
    "Moves the cursor to the last column of the last line of the file.",

/* 44 MARKPLACE	*/
    "Remembers the current line and column position so that it "
    "can be resumed later with a Goto Mark command. Up to 10 "
    "positions can be remembered.",

/* 45 GOTOMARK	*/
    "Goes to the next position remembered using the Mark Place "
    "command.",

/* 46 SETBMARK	*/
    "Sets a block marker at the current position.",

/* 47 SETCMARK	*/
    "Sets a column marker at the current position.",

/* 48 COPYTEXT	*/
    "Copies the currently marked text to the scrap buffer without "
    "deleting it.",

/* 49 BPASTE	*/
    "Pastes the contents of the scrap buffer as a block at the"
    " current position in the file.",

/* 50 CPASTE	*/
    "Pastes the contents of the scrap buffer as columns at the"
    " current position in the file, adding spaces if necessary"
    " to ensure all columns pasted are aligned.",

/* 51 FSEARCH	*/
    "Searches forward for the next occurrence of a specified string."
    " subject to the constraints specified in the Options menu.",

/* 52 BSEARCH	*/
    "Searches backward for the next occurrence of a specified string,"
    " subject to the constraints specified in the Options menu.",

/* 53 TRANSLATE	*/
    "Searches for the next occurrence of a specified string, and "
    "replaces it with a different specified string. At each match "
    "you will be prompted whether you want to replace the string, "
    "or whether you want a global replacement. A global replacement"
    " will replace all occurrences without prompting you first for "
    "confirmation.~Replacements are made subject to the matching "
    "constraints in the option menu. If you wish to stop the process"
    " at some stage, press ESC when prompted. WARNING: the translate"
    " command is NOT undoable!",

/* 54 LMATCH	*/
    "Searches left for a character matching any one of a specified"
    " set of characters. You may change the set of matching "
    "characters from the Options menu. This is most useful as a "
    "simple form of parenthesis matching.",

/* 55 RMATCH	*/
    "Searches right for a character matching any one of a specified"
    " set of characters. You may change the set of matching "
    "characters from the Options menu. This is most useful as a "
    "simple form of parenthesis matching.",

/* 56 UNDO		*/
    "Restores the current line to what it was when the cursor first "
    "moved on to it.",

/* 57 RECMACRO	*/
    "Records a keystroke macro. Up to ten sequences of "
    "256 keystrokes may be assigned to the keys Alt-1 to Alt-0. "
    "More help is available when you actually execute this "
    "command.",

/* 58 CRIT_QUIT	*/
    "The PEW is currently in a critical section. It is not "
    "safe for it to write log files until it is out of this "
    "section. Thus you may not return to the editor yet. First "
    "execute the specification a bit more (for example, until"
    " the end of this transition with F3) and then try again.",
/* 59 ALZTIME	*/
    "Please enter the execution timelimit for each execution"
    " to be used during analysis.",
/* 60 IDENTONLY	*/
    "Toggles identifier only matching on/off. If this option is on, "
    "a search string will only be matched if it is immediately "
    "surrounded by non-alphanumeric characters. This will prevent "
    "a match if the search string is a substring of some identifier."
    " It is primarily useful for global translations of a particular "
    "identifier name.",

/* 61 FILECH	*/
    "The current editor workfile has been altered since it was last"
    " saved. Press Y to save it, N if you don't want to save it (in "
    "which case the changes will be lost), or ESC to cancel.",

/* 62 TRANS	*/
    "Press Y to replace the string, N to skip to next match, G "
    "(global) to replace all matches without prompting for "
    "confirmation, or ESC to cancel.",

/* 63 READSCRAP    */
    "This command allows you to load any file into the scrap buffer."
    " The scrap can then be pasted as normal.",

/* 64 WRITESCRAP   */
    "This command allows you to write the contents of the scrap buffer"
    " to a file on disk.",

/* 65 LANGHELP	*/
    "This command searches (and displays if found) for text in the file"
    " LANGHELP using the word in the editor buffer at which the cursor"
    " is positioned as the index. It is intended to be used to provide"
    " language help facilities for programming languages such as Estelle.",

/* 66 TEMPLATE	*/
    "This command searches for text in the file TEMPLATE using the word"
    " in the editor buffer at which the cursor is positioned as the "
    "index. If such text is found, it replaces the index word. This is"
    " intended to be used as, for example, a \"poor man's\" syntax-"
    "directed editor.",

/* 67 DELLINE	*/
    "This command deletes the current line of the buffer.",

/* 68 BACKSPACE	*/
    "This key deletes the character to the immediate left of the cursor."
    " If the cursor is at the leftmost column, the the current line is "
    "joined to the line preceding it. If a SHIFT key is held down at "
    "the same time as backspace is pressed, then all space characters "
    "up to the first tab stop position will be deleted. In the latter "
    "case, a non-space character will end the deletion.",
/* 69 EXEC_ENVIRONMENT */
    "You are now in the PEW Execution Environment. To return to the "
    "editor, use Ctrl-E or Alt-Z. While in the execution environment, "
    "you can control execution by using the keys:~~F2 - Step on Statement~"
    "F3 - Step on Transition~F4 - Step on Iteration~F9 - Execute "
    "until Breakpoint~F10 - Quit to Editor"
    "~~The F7 key lets you see the I/O window, while the F8 key turns"
    " the log file on and off.~When not executing, you "
    "have four windows on the screen: the Line window, the Transition "
    "window, the Child window, and the IP window. The message line at "
    "the bottom of the screen tells you what process is active; the "
    "windows all show information about that process. At any stage, one "
    "of these windows is active, and you may move a highlight bar up and"
    " down within the window by using the cursor keys, Home, End, PgUp"
    " and PgDn. Pressing ENTER will invoke a menu for the"
    " currently highlighted item (except in the child window, while F6 "
    "will switch to a different non-empty window. During execution (F9) "
    "window updating may be enabled or disabled by entering an appropriate"
    " inter-statement animation delay. You can also enter an execution "
    "timelimit; the default is to continue until a keypress or breakpoint."
    " If the display suddenly begins being updated while still executing, you"
    " are running out of memory.~To examine a different process,"
    " the following keys can be used:~~"
    "F5 - Examine parent process~ENTER - Examine child process~~"
    "The last command is only available when the highlight bar is "
    "on an initialised module variable in the Child window."
    "~~The transition window shows"
    " each transition, together with information about its WHEN, "
    "PROVIDED and DELAY clauses, whether it is enabled, and whether "
    "it has been selected for execution; this information is represented "
    "by 1 (TRUE), 0 (FALSE) and - (not applicable). In each case, the "
    "information is preceded by a letter, namely W(hen), D(elay), "
    "P(rovided), E(nabled) and S(elected). If the transition is currently "
    "being executed, it has an asterisk on the left-hand side.~~The "
    "IP window shows which interactions are queued at each IP ready "
    "for WHEN clauses, as well as the length of each queue."
    " If the queue is long, you can use the left and right arrow keys"
    " to scroll back and forth along it. The length displayed is the"
    " number of interactions queued from the first displayed; it thus"
    " varies during this scrolling process. Pressing DEL on an IP"
    "will delete the first displayed interaction from the reception"
    " queue of the IP. You can use this to manually 'lose' interactions"
    " queued at an IP."
    "~~Some of this information only becomes available once the relevant "
    "process has been initialised and has begun execution; for example "
    "you may see IP details being displayed without the IP names "
    "being shown, as IP names are only known to the interpreter once "
    "the IP is attached or connected.",
/* 70 ANIMATE	*/
    "You may specify an animate delay in milliseconds at this prompt."
    " The PEW will delay for this length of time between each Estelle "
    "statement. Once execution is started,"
    " it continues until a breakpoint, keypress (although "
    "keypresses are ignored during clause evaluation) or the"
    " execution timelimit (next prompt) is reached.~If you "
    "specify an animate delay of zero (the default), screen "
    "refreshing will be disabled except for the time. This is"
    " a much faster way of running simulations.",
/* 71 SETLNBRKPT	*/
    "This action sets a line breakpoint on the currently highlighted "
    "line if this is allowed, and exits the line debug menu."
    " Before using this action to set a breakpoint, you should set the "
    " breakpoint details. If you make a mistake, you can edit the "
    "currently specified breakpoints by selecting View Breakpoints from"
    " any of the window menus.",
/* 72 PASSCNT	*/
    "The pass count is the number of times this breakpoint must be "
    "enabled before the breakpoint action is performed. ",
/* 73 BRKPROCESS	*/
    "This option specified whether a breakpoint should be set for the"
    " current process only, or for all processes of the same body type"
    " that have the same parent as the current process.",
/* 74 BRKACTION	*/
    "This option specifies the action to be taken when the breakpoint "
    "matures. The actions can be one of:~~* Activate some other suspended"
    " breakpoint~* Stop execution and return to user control~* Dump one or "
    "more browser panes to the log file.",
/* 75 BRKOTHER	*/
    "This option specifies which suspended breakpoint should be "
    "reactivated if the breakpoint action is 'Activate'. The breakpoint "
    "must be identified by its number, which can be found by inspecting "
    "the breakpoint window.",
/* 76 DUMPPANE	*/
    "This option specifies which panes should be dumped to the log file"
    " if the breakpoint action is 'Dump'. The dumped panes may contain "
    "slightly different information to their screen counterparts.",
/* 77 DUMPPROCESS	*/
    "This option specifies which processes should have their panes "
    "dumped to the log file if the breakpoint action is 'Dump'. The "
    "four possibilities are:~* Current process~* Current process"
    " and all peers of the same type~* Current process and all "
    "its peers~* All processes.",
/* 78 TRANSBRKTYPES	*/
    "Breakpoints on transitions can be set on either of the conditions:"
    "~~* Breakpoint enabled~* Breakpoint about to be executed.~~The first"
    " type is checked while the scheduler performs clause evaluation "
    "and is the only way that clause evaluation can be interrupted.",
/* 79 SETTRANSBRKPT	*/
    "This command lets you set a breakpoint on a transition. When"
    " you press enter, you will be taken to the breakpoint menu "
    "where you specify details of this breakpoint. You can obtain "
    "additional help there.",
/* 80 VIEWTRANSSTATS	*/
    "This command lets you view the statistics associated with the "
    "currently highlighted breakpoint.",
/* 81 SETIPBRKPT	*/
    "This command sets a breakpoint which is enabled when an "
    "OUTPUT statement refering to the currently highlighted "
    "interaction point is executed.",
/* 82 VIEWIPSTATS	*/
    "This command displays the statistics currently associated with"
    " the currently highlighted interaction point.",
/* 83 VIEWCEP	*/
    "This command changes the process browser to display the process"
    " which has an interaction point connected or attached to the "
    "currently highlighted one that is a connection endpoint. The "
    "method used is to display the 'downattach' Ip of the current"
    " Ip if this is different to the Ip itself; otherwise the "
    "'downattach' of the 'upperattach' of the Ip is diplayed.",
/* 84 TIMELIMIT	*/
    "You may specify a time at which the execution should be stopped"
    " and control returned to the user. To execute continuously until "
    "a breakpoint, enter a time limit of zero.",
/* 85 BRKSUSPEND */
    "This option allows you to set whether the breakpoint is currently"
    " active or suspended.",
/* 86 VIEWBRKPTS */
    "This option displays a window of the currently set breakpoints. "
    "You may scroll up and down in the window with the up arrow and "
    "down arrow cursor keys. By moving the highlight on to a breakpoint,"
    " you may edit the breakpoint by pressing ENTER or RETURN, or "
    "delete it by pressing DEL.",
/* 87 BRKPTDEL	*/
    "Press Y to delete this breakpoint, N to cancel, or A to delete"
    " ALL breakpoints.",
/* 88 CONFIRMDEL	*/
    "This breakpoint is refered to by the actions of other breakpoints;"
    " that is, there are other breakpoints whose actions include "
    "activating this breakpoint. If you still wish to delete the "
    "breakpoint, press Y; to cancel the deletion, press N. In the "
    "former case, the breakpoints involved will have their reactivate "
    "actions cancelled.",
/* 89 LOADLOG	*/
    "During execution and when you exit the interpreter, statistics"
    " are written to a file named ESTELLE.LOG. This menu option is"
    " provided as a shortcut to allows you to examine a log after "
    "executing a specification.",
/* 90 BRKPTREADY	*/
    "Selecting this option adds a new breakpoint or changes an "
    "existing breakpoint to have the options currently set in this"
    " menu. If you wish to exit this menu without making any changes,"
    " press ESC.",
/* 91 BRKACTION	*/
    "This option specifies whether the interpreter should switch back"
    " to step mode from execute mode, or continue with execution. "
    "If you specify 'Stop', any other actions specified will be "
    "performed before execution stops.",
/* 92 BRKRESET	*/
    "This option specifies whether the breakpoint should remain active"
    " or be suspended after maturing. If you select 'Reset', it will"
    " remain active and mature again after being enabled for the "
    "number of times specified in its pass count.",
/* 93 BRKACTIVATE	*/
    "If you select 'Yes' for this option, the breakpoint specified "
    "below in the 'Breakpoint' field will be reactivated by this "
    "breakpoint as part of its action. Specifying 0 for the breakpoint"
    " will reactivate all suspended breakpoints.",
/* 94 TRANSSTATS */
    "The transition statistics window shows:~~* The state transition~"
    "* The number of times enabled~* The number of times fired (executed)"
    "~* The time when first executed~* The most recent execution time"
    "~* The longest delay between being enabled and executed~* The "
    "mean delay between being enabled and fired~* The number of times"
    " the PROVIDED clause was enabled~* The number of times the WHEN"
    " clause was enabled.",
/* 95 IPSTATS	*/
    "The interaction point statistics window shows:~~* The IP name"
    "~* The total number of interactions dequeued through^     this IP"
    " (through WHEN clauses).~* The mean length of the reception"
    " queue^     attached to this IP~* The maximum length reached by"
    " this queue~* The mean time spent by dequeued interactions in^"
    "     the queue~* The maximum such time.",
/* 96 BRKPTBROWSE */
    "You are now in the breakpoint browser. You can move the highlight"
    " up and down with the up and down cursor keys, to select which "
    "breakpoint you would like to view, edit or delete. Once you "
    "have selected the breakpoint, you can delete it by pressing DEL."
    " You will be asked to confirm deletion; at this point you may "
    "also press 'A' which will delete all breakpoints.~If you press"
    " ENTER or RETURN, the breakpoint menu will be activated for this"
    " breakpoint which will allow you to view or change the settings"
    " for the highlighted breakpoint.~To exit the breakpoint browser"
    " you must press ESC.",
/* 97 DELINTR	*/
    "Press 'Y' or 'N' to specify whether you wish to delete the first"
    " DISPLAYED interaction in the reception queue of the currently "
    "highlighted IP. You can use this to manually 'lose' interactions"
    " queued at an IP. Note that if you have moved through the list"
    " of interactions with the right and left cursor keys then the"
    " interaction to be deleted will not necessarily be the one"
    " at the head of the queue.",
/* 98 QUITEXEC	*/
    "Press 'Y' to quit to the editor, any other key to continue.",
/* 99 RANDOMSEED	*/
    "This option allows you to reseed the internal random number"
    " generator. It is useful if you wish to replicate an execution"
    " of a specification exactly.",
/* 100 ALZINDEP	*/
    "This option lets you specify the name of the independent"
    " variable which is to be varied during analysis. The "
    "specification will be compiled and executed for each value"
    " of this variable, and during each compilation it will be "
    "treated as though it is a constant declared at the highest "
    "scope level.",
/* 101 ALZFROM	*/
    "This option lets you specify the starting value for the "
    "independent variable for analysis.",
/* 102 ALZTO	*/
    "This option lets you specify the ending value for the "
    "independent variable for analysis.",
/* 103 ALZSTEP	*/
    "This option lets you specify the increment to be used "
    "for the independent variable for analysis between each"
    " recompilation.",
/* 104 ALZEDDEP	*/
    "This option lets you edit the new expressions to be tabulated"
    " during analysis. Expressions may use the operators +, -"
    ", * and /, as well as parentheses, and should be made up "
    "of integer constants, transition names, and the special "
    "name TIME. You should also name the dependent. For example"
    ":~~THRUPUT=SENDS/TIME~~The usual editing commands can be "
    "used. To clear the current line, use ESC. To exit the "
    "expression editor, use the ENTER key. Up to 8 expressions"
    " can be entered, and each can be up to 72 characters in "
    "length. Expressions may NOT be split over more than one"
    "line. See also the help for the Analyze menu.",
/* 105 ALZGO	*/
    "This option lets you specify the execution time limit for "
    "each execution done during analysis, and also begins the "
    "analysis process.",
/* 106 FULLSPEED */
    "If you request full speed, no breakpoint checking is done, "
    "transition clauses are short-circuit evaluated, and no "
    "bad interaction checking is done. This is to speed up "
    "execution to a maximum.",
/* 107 NUMSTATS	*/
    "You can specify how many statistics entries you would like to"
    " view. The first line displayed corresponds to the line in the "
    "window which is highlighted. At most 20 entries can be displayed"
    " at a time.",
/* 108 GENTRACE */
    "This option specifies whether you want to generate a trace of the"
    " interactions that are output during execution. If so, the trace "
    "is sent to the file TRACE.",


    NULL, NULL
};


long HelpOffsets[MAXHELP];

#define HLP_TEXTFILE	"PEW_HELP.ITX"
#define HLP_INDEXFILE	"PEW_HELP.IDX"

main()
{
    FILE *fp;
    short i, j;
    fp = fopen(HLP_TEXTFILE, "wb");
    i = 0;
    while (HelpText[i] != NULL)
    {
	short len = strlen(HelpText[i]);
	HelpOffsets[i] = ftell(fp);
	fwrite(&len, sizeof(short), 1, fp);
	fwrite(HelpText[i], sizeof(char), len, fp);
	i++;
	if (i >= MAXHELP)
	{
	    fprintf(stderr, "makehelp: Maximum allowed help entries exceeded\n");
	    exit(-1);
	}
	HelpOffsets[i] = 0l;
    }
    fclose(fp);
    fp = fopen(HLP_INDEXFILE, "wb");
    j = 0;
    while (j <= i)
	fwrite(&HelpOffsets[j++], sizeof(long), 1, fp);
    fclose(fp);
}
