
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "config.h"
#include <snapper/Factory.h>
#include <snapper/Snapper.h>
#include <snapper/Snapshot.h>
#include <snapper/Comparison.h>
#include <snapper/File.h>
#include <snapper/AppUtil.h>
#include <snapper/SnapperTmpl.h>
#include <snapper/Compare.h>
#include <snapper/Enum.h>
#include <snapper/AsciiFile.h>

#include "utils/Table.h"
#include "utils/GetOpts.h"

using namespace snapper;
using namespace std;


typedef void (*cmd_fnc)();
map<string, cmd_fnc> cmds;

GetOpts getopts;

bool quiet = false;
bool verbose = false;
string config_name = "root";
bool disable_filters = false;

Snapper* sh = NULL;


void
help_list_configs()
{
    cout << _("  List configs:") << endl
	 << _("\tsnapper list-configs") << endl
	 << endl;
}


void
command_list_configs()
{
    getopts.parse("list-configs", GetOpts::no_options);
    if (getopts.hasArgs())
    {
	cerr << _("Command 'list-configs' does not take arguments.") << endl;
	exit(EXIT_FAILURE);
    }

    Table table;

    TableHeader header;
    header.add(_("Config"));
    header.add(_("Subvolume"));
    table.setHeader(header);

    try
    {
	list<ConfigInfo> config_infos = Snapper::getConfigs();
	for (list<ConfigInfo>::const_iterator it = config_infos.begin(); it != config_infos.end(); ++it)
	{
	    TableRow row;
	    row.add(it->config_name);
	    row.add(it->subvolume);
	    table.add(row);
	}
    }
    catch (const ListConfigsFailedException& e)
    {
	cerr << sformat(_("Listing configs failed (%s)."), e.what()) << endl;
	exit(EXIT_FAILURE);
    }

    cout << table;
}


void
help_create_config()
{
    cout << _("  Create config:") << endl
	 << _("\tsnapper create-config <subvolume>") << endl
	 << endl
	 << _("    Options for 'create-config' command:") << endl
	 << _("\t--template, -t <name>\t\tName of config template to use.") << endl
	 << endl;
}


void
command_create_config()
{
    const struct option options[] = {
	{ "template",		required_argument,	0,	't' },
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("create-config", options);
    if (getopts.numArgs() != 1)
    {
	cerr << _("Command 'create-config' needs one argument.") << endl;
	exit(EXIT_FAILURE);
    }

    string subvolume = getopts.popArg();

    string template_name = "default";

    GetOpts::parsed_opts::const_iterator opt;

    if ((opt = opts.find("template")) != opts.end())
	template_name = opt->second;

    try
    {
	Snapper::addConfig(config_name, subvolume, template_name);
    }
    catch (const AddConfigFailedException& e)
    {
	cerr << sformat(_("Creating config failed (%s)."), e.what()) << endl;
	exit(EXIT_FAILURE);
    }
}


Snapshots::iterator
readNum(const string& str)
{
    Snapshots& snapshots = sh->getSnapshots();

    unsigned int num = 0;
    if (str != "current")
	str >> num;

    Snapshots::iterator snap = snapshots.find(num);
    if (snap == snapshots.end())
    {
	cerr << sformat(_("Snapshot '%u' not found."), num) << endl;
	exit(EXIT_FAILURE);
    }

    return snap;
}


void
help_list()
{
    cout << _("  List snapshots:") << endl
	 << _("\tsnapper list") << endl
	 << endl
	 << _("    Options for 'list' command:") << endl
	 << _("\t--type, -t <type>\t\tType of snapshots to list.") << endl
	 << endl;
}


void
command_list()
{
    const struct option options[] = {
	{ "type",		required_argument,	0,	't' },
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("list", options);
    if (getopts.hasArgs())
    {
	cerr << _("Command 'list' does not take arguments.") << endl;
	exit(EXIT_FAILURE);
    }

    enum ListMode { LM_ALL, LM_SINGLE, LM_PRE_POST };
    ListMode list_mode = LM_ALL;

    GetOpts::parsed_opts::const_iterator opt;

    if ((opt = opts.find("type")) != opts.end())
    {
	if (opt->second == "all")
	    list_mode = LM_ALL;
	else if (opt->second == "single")
	    list_mode = LM_SINGLE;
	else if (opt->second == "pre-post")
	    list_mode = LM_PRE_POST;
	else
	{
	    cerr << _("Unknown type of snapshots.") << endl;
	    exit(EXIT_FAILURE);
	}
    }

    Table table;

    switch (list_mode)
    {
	case LM_ALL:
	{
	    TableHeader header;
	    header.add(_("Type"));
	    header.add(_("#"));
	    header.add(_("Pre #"));
	    header.add(_("Date"));
	    header.add(_("Cleanup"));
	    header.add(_("Name"));
	    header.add(_("Description"));
	    table.setHeader(header);

	    const Snapshots& snapshots = sh->getSnapshots();
	    for (Snapshots::const_iterator it1 = snapshots.begin(); it1 != snapshots.end(); ++it1)
	    {
		TableRow row;
		row.add(toString(it1->getType()));
		row.add(decString(it1->getNum()));
		row.add(it1->getType() == POST ? decString(it1->getPreNum()) : "");
		row.add(it1->isCurrent() ? "" : datetime(it1->getDate(), false, false));
		row.add(it1->getCleanup());
		row.add(it1->getName());
		row.add(it1->getDescription());
		table.add(row);
	    }
	}
	break;

	case LM_SINGLE:
	{
	    TableHeader header;
	    header.add(_("#"));
	    header.add(_("Date"));
	    header.add(_("Name"));
	    header.add(_("Description"));
	    table.setHeader(header);

	    const Snapshots& snapshots = sh->getSnapshots();
	    for (Snapshots::const_iterator it1 = snapshots.begin(); it1 != snapshots.end(); ++it1)
	    {
		if (it1->getType() != SINGLE)
		    continue;

		TableRow row;
		row.add(decString(it1->getNum()));
		row.add(it1->isCurrent() ? "" : datetime(it1->getDate(), false, false));
		row.add(it1->getName());
		row.add(it1->getDescription());
		table.add(row);
	    }
	}
	break;

	case LM_PRE_POST:
	{
	    TableHeader header;
	    header.add(_("Pre #"));
	    header.add(_("Post #"));
	    header.add(_("Pre Date"));
	    header.add(_("Post Date"));
	    header.add(_("Name"));
	    header.add(_("Description"));
	    table.setHeader(header);

	    const Snapshots& snapshots = sh->getSnapshots();
	    for (Snapshots::const_iterator it1 = snapshots.begin(); it1 != snapshots.end(); ++it1)
	    {
		if (it1->getType() != PRE)
		    continue;

		Snapshots::const_iterator it2 = snapshots.findPost(it1);
		if (it2 == snapshots.end())
		    continue;

		TableRow row;
		row.add(decString(it1->getNum()));
		row.add(decString(it2->getNum()));
		row.add(it1->isCurrent() ? "" : datetime(it1->getDate(), false, false));
		row.add(it2->isCurrent() ? "" : datetime(it2->getDate(), false, false));
		row.add(it1->getName());
		row.add(it1->getDescription());
		table.add(row);
	    }
	}
	break;
    }

    cout << table;
}


void
help_create()
{
    cout << _("  Create snapshot:") << endl
	 << _("\tsnapper create") << endl
	 << endl
	 << _("    Options for 'create' command:") << endl
	 << _("\t--type, -t <type>\t\tType for snapshot.") << endl
	 << _("\t--pre-number <number>\t\tNumber of corresponding pre snapshot.") << endl
	 << _("\t--description, -d <description>\tDescription for snapshot.") << endl
	 << _("\t--print-number, -p\t\tPrint number of created snapshot.") << endl
	 << _("\t--cleanup-algorithm, -c\t\tCleanup algorithm for snapshot.") << endl
	 << endl;
}


void
command_create()
{
    const struct option options[] = {
	{ "type",		required_argument,	0,	't' },
	{ "pre-number",		required_argument,	0,	0 },
	{ "description",	required_argument,	0,	'd' },
	{ "print-number",	no_argument,		0,	'p' },
	{ "cleanup-algorithm",	required_argument,	0,	'c' },
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("create", options);
    if (getopts.numArgs() != 1)
    {
	cerr << _("Command 'create' needs one argument.") << endl;
	exit(EXIT_FAILURE);
    }

    SnapshotType type = SINGLE;
    Snapshots::const_iterator snap1;
    string description;
    bool print_number = false;
    string cleanup;

    GetOpts::parsed_opts::const_iterator opt;

    if ((opt = opts.find("type")) != opts.end())
    {
	if (!toValue(opt->second, type, SINGLE))
	{
	    cerr << _("Unknown type of snapshot.") << endl;
	    exit(EXIT_FAILURE);
	}
    }

    if ((opt = opts.find("pre-number")) != opts.end())
	snap1 = readNum(opt->second);

    if ((opt = opts.find("description")) != opts.end())
	description = opt->second;

    if ((opt = opts.find("print-number")) != opts.end())
	print_number = true;

    if ((opt = opts.find("cleanup-algorithm")) != opts.end())
	cleanup = opt->second;

    string name = getopts.popArg();

    switch (type)
    {
	case SINGLE: {
	    Snapshots::iterator snap1 = sh->createSingleSnapshot(name, description);
	    snap1->setCleanup(cleanup);
	    if (print_number)
		cout << snap1->getNum() << endl;
	} break;

	case PRE: {
	    Snapshots::iterator snap1 = sh->createPreSnapshot(name, description);
	    snap1->setCleanup(cleanup);
	    if (print_number)
		cout << snap1->getNum() << endl;
	} break;

	case POST: {
	    Snapshots::iterator snap2 = sh->createPostSnapshot(name, snap1);
	    snap2->setCleanup(cleanup);
	    if (print_number)
		cout << snap2->getNum() << endl;
	    sh->startBackgroundComparsion(snap1, snap2);
	} break;
    }
}


void
help_modify()
{
    cout << _("  Modify snapshot:") << endl
	 << _("\tsnapper modify <number>") << endl
	 << endl
	 << _("    Options for 'modify' command:") << endl
	 << _("\t--description, -d <description>\tDescription for snapshot.") << endl
	 << endl;
}


void
command_modify()
{
    const struct option options[] = {
	{ "description",	required_argument,	0,	'd' },
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("modify", options);
    if (getopts.numArgs() != 1)
    {
	cerr << _("Command 'modify' needs one argument.") << endl;
	exit(EXIT_FAILURE);
    }

    Snapshots::iterator snapshot = readNum(getopts.popArg());

    GetOpts::parsed_opts::const_iterator opt;

    if ((opt = opts.find("description")) != opts.end())
	snapshot->setDescription(opt->second);
}


void
help_delete()
{
    cout << _("  Delete snapshot:") << endl
	 << _("\tsnapper delete <number>") << endl
	 << endl;
}


void
command_delete()
{
    getopts.parse("delete", GetOpts::no_options);
    if (!getopts.hasArgs())
    {
	cerr << _("Command 'delete' needs at least one argument.") << endl;
	exit(EXIT_FAILURE);
    }

    // TODO: Add support to delete using names
    while (getopts.hasArgs())
    {
	Snapshots::iterator snapshot = readNum(getopts.popArg());
	sh->deleteSnapshot(snapshot);
    }
}


void
help_diff()
{
    cout << _("  Comparing snapshots:") << endl
	 << _("\tsnapper diff <number1> <number2>") << endl
	 << endl
	 << _("    Options for 'diff' command:") << endl
	 << _("\t--output, -o <file>\t\tSave diff to file.") << endl
	 << _("\t--file, -f <file>\t\tRun diff for file.") << endl
	 << endl;
}


void
command_diff()
{
    const struct option options[] = {
	{ "output",		required_argument,	0,	'o' },
	{ "file",		required_argument,	0,	'f' },
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("diff", options);
    if (getopts.numArgs() != 2)
    {
	cerr << _("Command 'diff' needs two arguments.") << endl;
	exit(EXIT_FAILURE);
    }

    GetOpts::parsed_opts::const_iterator opt;

    Snapshots::const_iterator snap1 = readNum(getopts.popArg());
    Snapshots::const_iterator snap2 = readNum(getopts.popArg());

    Comparison comparison(sh, snap1, snap2);

    const Files& files = comparison.getFiles();

    Files::const_iterator tmp = files.end();

    if ((opt = opts.find("file")) != opts.end())
    {
	tmp = files.find(opt->second);
	if (tmp == files.end())
	{
	    cerr << sformat(_("File '%s' not included in diff."), opt->second.c_str()) << endl;
	    exit(EXIT_FAILURE);
	}
    }

    FILE* file = stdout;

    if ((opt = opts.find("output")) != opts.end())
    {
	file = fopen(opt->second.c_str(), "w");
	if (!file)
	{
	    cerr << sformat(_("Opening file '%s' failed."), opt->second.c_str()) << endl;
	    exit(EXIT_FAILURE);
	}
    }

    if (tmp == files.end())
    {
	for (Files::const_iterator it = files.begin(); it != files.end(); ++it)
	    fprintf(file, "%s %s\n", statusToString(it->getPreToPostStatus()).c_str(), it->getName().c_str());
    }
    else
    {
	vector<string> lines = tmp->getDiff("--unified --new-file");
	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	    fprintf(file, "%s\n", it->c_str());
    }

    if (file != stdout)
	fclose(file);
}


void
help_rollback()
{
    cout << _("  Rollback snapshots:") << endl
	 << _("\tsnapper rollback <number1> <number2>") << endl
	 << endl
	 << _("    Options for 'rollback' command:") << endl
	 << _("\t--file, -f <file>\t\tRead files to rollback from file.") << endl
	 << endl;
}


void
command_rollback()
{
    const struct option options[] = {
	{ "file",		required_argument,	0,	'f' },
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("rollback", options);
    if (getopts.numArgs() != 2)
    {
	cerr << _("Command 'rollback' needs two arguments.") << endl;
	exit(EXIT_FAILURE);
    }

    Snapshots::const_iterator snap1 = readNum(getopts.popArg());
    Snapshots::const_iterator snap2 = readNum(getopts.popArg());

    FILE* file = NULL;

    GetOpts::parsed_opts::const_iterator opt;

    if ((opt = opts.find("file")) != opts.end())
    {
	file = fopen(opt->second.c_str(), "r");
	if (!file)
	{
	    cerr << sformat(_("Opening file '%s' failed."), opt->second.c_str()) << endl;
	    exit(EXIT_FAILURE);
	}
    }

    Comparison comparison(sh, snap1, snap2);

    Files& files = comparison.getFiles();

    if (file)
    {
	AsciiFileReader asciifile(file);

	string line;
	while (asciifile.getline(line))
	{
	    if (line.empty())
		continue;

	    string name = line;

	    // strip optional status
	    if (name[0] != '/')
	    {
		string::size_type pos = name.find(" ");
		if (pos == string::npos)
		    continue;

		name.erase(0, pos + 1);
	    }

	    Files::iterator it = files.find(name);
	    if (it == files.end())
	    {
		cerr << sformat(_("File '%s' not found in diff."), name.c_str()) << endl;
		exit(EXIT_FAILURE);
	    }

	    it->setRollback(true);
	}
    }
    else
    {
	for (Files::iterator it = files.begin(); it != files.end(); ++it)
	    it->setRollback(true);
    }

    RollbackStatistic rs = comparison.getRollbackStatistic();

    if (rs.empty())
    {
	cout << "nothing to do" << endl;
	return;
    }

    cout << "create:" << rs.numCreate << " modify:" << rs.numModify << " delete:" << rs.numDelete
	 << endl;

    comparison.doRollback();
}


void
help_cleanup()
{
    cout << _("  Cleanup snapshots:") << endl
	 << _("\tsnapper cleanup <cleanup-algorithm>") << endl
	 << endl;
}


void
command_cleanup()
{
    const struct option options[] = {
	{ 0, 0, 0, 0 }
    };

    GetOpts::parsed_opts opts = getopts.parse("cleanup", options);
    if (getopts.numArgs() != 1)
    {
	cerr << _("Command 'cleanup' needs one arguments.") << endl;
	exit(EXIT_FAILURE);
    }

    string cleanup = getopts.popArg();

    if (cleanup == "number")
    {
	sh->doCleanupNumber();
    }
    else if (cleanup == "timeline")
    {
	sh->doCleanupTimeline();
    }
    else if (cleanup == "empty-pre-post")
    {
	sh->doCleanupEmptyPrePost();
    }
    else
    {
	cerr << sformat(_("Unknown cleanup algorithm '%s'."), cleanup.c_str()) << endl;
	exit(EXIT_FAILURE);
    }
}


void
command_help()
{
    getopts.parse("help", GetOpts::no_options);
    if (getopts.hasArgs())
    {
	cerr << _("Command 'help' does not take arguments.") << endl;
	exit(EXIT_FAILURE);
    }

    cout << _("usage: snapper [--global-options] <command> [--command-options] [command-arguments]") << endl
	 << endl;

    cout << _("    Global options:") << endl
	 << _("\t--quiet, -q\t\t\tSuppress normal output.") << endl
	 << _("\t--verbose, -v\t\t\tIncrease verbosity.") << endl
	 << _("\t--table-style, -t <style>\tTable style (integer).") << endl
	 << _("\t--config, -c <name>\t\tSet name of config to use.") << endl
	 << _("\t--disable-filters\t\tDisable filters.") << endl
	 << _("\t--version\t\t\tPrint version and exit.") << endl
	 << endl;

    help_list_configs();
    help_create_config();
    help_list();
    help_create();
    help_modify();
    help_delete();
    help_diff();
    help_rollback();
    help_cleanup();
}


struct CompareCallbackImpl : public CompareCallback
{
    void start() { cout << _("comparing snapshots...") << flush; }
    void stop() { cout << " " << _("done") << endl; }
};

CompareCallbackImpl compare_callback_impl;


struct RollbackCallbackImpl : public RollbackCallback
{
    void start() { cout << _("running rollback...") << endl; }
    void stop() { cout << _("rollback done") << endl; }

    void createInfo(const string& name)
	{ if (verbose) cout << sformat(_("creating %s"), name.c_str()) << endl; }
    void modifyInfo(const string& name)
	{ if (verbose) cout << sformat(_("modifying %s"), name.c_str()) << endl; }
    void deleteInfo(const string& name)
	{ if (verbose) cout << sformat(_("deleting %s"), name.c_str()) << endl; }

    void createError(const string& name)
	{ cerr << sformat(_("failed to create %s"), name.c_str()) << endl; }
    void modifyError(const string& name)
	{ cerr << sformat(_("failed to modify %s"), name.c_str()) << endl; }
    void deleteError(const string& name)
	{ cerr << sformat(_("failed to delete %s"), name.c_str()) << endl; }
};

RollbackCallbackImpl rollback_callback_impl;


int
main(int argc, char** argv)
{
    setlocale(LC_ALL, "");

    initDefaultLogger();

    cmds["list-configs"] = command_list_configs;
    cmds["create-config"] = command_create_config;
    cmds["list"] = command_list;
    cmds["create"] = command_create;
    cmds["modify"] = command_modify;
    cmds["delete"] = command_delete;
    cmds["diff"] = command_diff;
    cmds["rollback"] = command_rollback;
    cmds["cleanup"] = command_cleanup;
    cmds["help"] = command_help;

    const struct option options[] = {
	{ "quiet",		no_argument,		0,	'q' },
	{ "verbose",		no_argument,		0,	'v' },
	{ "table-style",	required_argument,	0,	't' },
	{ "config",		required_argument,	0,	'c' },
	{ "disable-filters",	no_argument,		0,	0 },
	{ "version",		no_argument,		0,	0 },
	{ 0, 0, 0, 0 }
    };

    getopts.init(argc, argv);

    GetOpts::parsed_opts opts = getopts.parse(options);

    GetOpts::parsed_opts::const_iterator opt;

    if ((opt = opts.find("quiet")) != opts.end())
	quiet = true;

    if ((opt = opts.find("verbose")) != opts.end())
	verbose = true;

    if ((opt = opts.find("table-style")) != opts.end())
    {
	unsigned int s;
	opt->second >> s;
	if (s >= _End)
	{
	    cerr << sformat(_("Invalid table style %d."), s) << " "
		 << sformat(_("Use an integer number from %d to %d"), 0, _End - 1) << endl;
	    exit(EXIT_FAILURE);
	}
	Table::defaultStyle = (TableLineStyle) s;
    }

    if ((opt = opts.find("config")) != opts.end())
	config_name = opt->second;

    if ((opt = opts.find("disable-filters")) != opts.end())
	disable_filters = true;

    if ((opt = opts.find("version")) != opts.end())
    {
	cout << "snapper " << VERSION << endl;
	exit(EXIT_SUCCESS);
    }

    if (!getopts.hasArgs())
    {
	cerr << _("No command provided.") << endl
	     << _("Try 'snapper help' for more information.") << endl;
	exit(EXIT_FAILURE);
    }

    const char* command = getopts.popArg();
    map<string, cmd_fnc>::const_iterator cmd = cmds.find(command);
    if (cmd == cmds.end())
    {
	cerr << sformat(_("Unknown command '%s'."), command) << endl
	     << _("Try 'snapper help' for more information.") << endl;
	exit(EXIT_FAILURE);
    }

    if (cmd->first == "help" || cmd->first == "list-configs" || cmd->first == "create-config")
    {
	(*cmd->second)();
    }
    else
    {
	try
	{
	    sh = createSnapper(config_name, disable_filters);
	}
	catch (const ConfigNotFoundException& e)
	{
	    cerr << sformat(_("Config '%s' not found."), config_name.c_str()) << endl;
	    exit(EXIT_FAILURE);
	}
	catch (const InvalidConfigException& e)
	{
	    cerr << sformat(_("Config '%s' is invalid."), config_name.c_str()) << endl;
	    exit(EXIT_FAILURE);
	}

	if (!quiet)
	{
	    sh->setCompareCallback(&compare_callback_impl);
	    sh->setRollbackCallback(&rollback_callback_impl);
	}

	(*cmd->second)();

	deleteSnapper(sh);
    }

    exit(EXIT_SUCCESS);
}
