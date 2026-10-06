#!/usr/bin/perl -w
# parse-df.pl ${XeroxCedar}/release/Top/*.df
# parse-df.pl --debug --host:Cedar10.1=${XeroxCedar}/release /Cedar10.1/Top/Interpress.df

use strict;
use warnings;

use Time::Piece;

use lib $ENV{HOME}.'/bin/perlib';
use io::text qw(inhale_file);

my $endl = "\n";
my $cr = "\r";
my $dquot = '"';
my $squot = "'";
my $blank = ' ';
my $slash = '/';
my $endbrace = ']';
my $dot = '.';

# Unix / Epoch values :
# 0 : Thursday, January 1, 1970 at 12:00:00 AM
# -63158400 : Monday, January 1, 1968 at 12:00:00 AM UTC
# -63129600 : Monday, January 1, 1968 at 12:00:00 AM UTC-08:00
my $bt_adjust = (-24*60*60) + (-2*365*24*60*60); # 63,158,400 secs
my $bt_zone_adjust = (8*60*60); # 28,800 secs
my %tz_map = (
    'PDT' => '-0700',
    'PST' => '-0800'
);

my $basictime_fmt = '%d-%b-%y %H:%M:%S %z';

# use PerlIO::gzip;
# use Text::Tabs;
# use Digest::SHA qw(sha1_hex);
# use JSON qw(decode_json encode_json);
# use Data::Dumper;
# use Data::GUID;
use JSON::PP qw(encode_json);

if ($#ARGV < 0) {
    die(join($blank, 'usage:',
        '[-verbose]',
        'file.df[.gz] ...'
    ));
}

# flags
my $debug = 0;
my $trace = 0;
my $unknown = 0;

my $cedar = $ENV{XeroxCedar};
my %host;

my %freq;

# --
# these could be recompiled at any time:

# index of 'paths' by ifs_server
my %ifs_index;

# project graph, like smodel but import => (required-by) project
my %project_graph;

# local namespace (files) - file object table, f => fobj (vname)
my %locals;


# --

# my $sepr = '.!>+-$_~?';
my $vname_sepr = '.!>+';

# IsDelim(c) ((c) == '[' || (c) == ']' || (c) == '<' || (c) == '>' || (c) == '/')

# +sub>base.ext!version
## local namespace file
sub parse_vname {
    my ($vname) = @_;

    # remove 'generated' marker
    my $has_plus = ($vname =~ m|\+|);
    $vname =~ s|^\+||;

    my $last_delim = rindex($vname, '>');
    my ($subpath, $simple) = ('', $vname);

    if ($last_delim != -1) {
        $subpath = substr($vname, 0, $last_delim);
        $simple  = substr($vname, $last_delim + 1);
    }

    ## my @subpath = split('>', $vname);
    ## my $plain = pop @subpath;

    my ($plain, $v) = split(/!/, $simple, 2);
    $v //= '';

    # special case for dot-file(s)
    my ($base, $ext) = ($plain, '');
    if (($plain =~ m/\./) and (substr($plain, 0) ne '.')) {
        ($base, $ext) = split(/\./, $plain, 2);
        $ext //= '';
    }

    my %obj;
    $obj{has_plus} = $has_plus;
    $obj{subpath} = $subpath; # may contain internal '>'s
    $obj{version} = $v;
    $obj{base} = $base;
    $obj{ext} = $ext;
    $obj{owner} = ''; # ensured defined, set in context
    print STDERR (join($blank, 'local-file', $ext, $base, $v, $subpath, ($has_plus) ? '+' : ''), $endl) if $debug;

    return \%obj;
}

# table of abstract object : df (name) -> adf (obj)
# constructs the "reachable set" of sub-projects and 'referenceable' files
my %smodel;

sub resolve_df {
    my ($given) = @_;

    if ($given =~ m|^/|) {
        my ($dead, $ifs_server, @rest) = split($slash, $given);
        print STDERR (join($blank, 'ifs_server:', $ifs_server, @rest), $endl) if $debug;

        my $root = $host{$ifs_server};
        my $pathname = join($slash, $root, @rest);
        print STDERR (join($blank, 'root:', $root, 'pathname:', $pathname), $endl) if $debug;

        my $canon = "[".$ifs_server."]<".join(">", @rest);
        print STDERR (join($blank, 'canon:', $canon), $endl) if $debug;

        # get by with a little help from git
        my $goid = `git hash-object $pathname`;
        chomp($goid);
        print STDERR (join($blank, 'goid:', $goid, $pathname), $endl) if $debug;

        my %df;
        $df{pathname} = $pathname;
        $df{canon} = $canon;
        $df{goid} = $goid;
# FIXME
        $smodel{$pathname} = \%df; # ensure defined
        # $df{location} = $location;
        # $df{created} = $created;
        # $df{version} = $version;
        # $canon = ...;

        # [host]<path>module.df!vnum@mtime - path may include '>'

        return $smodel{$pathname}; # let's call this a df-ref
    }
}

sub parse_df {
    my ($filename) = @_;

    ## magic here:
    my $r = resolve_df($filename);
    # resolve_df must set 'pathname'

    my $body = $r->{body};
    if (defined $body) {
        print STDERR(join($blank, "parse_df: recursive entry", $filename), $endl) if $debug or $trace;
        return;
    }

    # a model (df file) has :
    # 'pathname` (thing that's 'inhaled') - determined by resolve_df
    # 'body' (text file, list of lines)
    # 'tree' abstract tree (what's in the lines)
    my $pathname = $r->{pathname};
    my @lines = inhale_file($pathname);
    # /Cedar10.1/Top/Interpress.df lines:
    print STDERR (join(' ', $pathname, 'lines:', $#lines+1), $endl) if $debug or $trace;

    ## UGH - inhale doesn't do the right thing for \r text files
    if (($#lines == 0) and ($lines[0] =~ m/\r/)) {
        @lines = split($cr, $lines[0]);
        print STDERR (join(' ', 'FIXUP:', $pathname, 'lines:', $#lines+1), $endl) if $debug or $trace;
    }

    my %tree;
    ## $r->{tree} = \%tree; # do this at the end
    $r->{body} = \@lines;

# FIXME:
    my $active_df = $pathname; # 'module' name

    # active context:
    my $pending_section;
    my $lineno = 0; # primarily for trace/debug

my $carry_over = '';

    # tree has a list of 'sections':
    my @sections = [];
    foreach my $orig (@lines) {
        chomp($orig);
        my $text = $orig; # copy?
        $lineno++;
        next if $text =~ m|^\s*$|; # blank lines
        next if $text =~ m|^\s*--|; # comment lines

## FIX : ugh, multi-line 'Using' clause
# --
        if (($text =~ m|^  Using \[|) and (substr($text, -1) ne $endbrace)) {
            $carry_over = $text;
            print STDERR ($endl, join($blank, $lineno, $carry_over), $endl) if $trace; # extra line break
            next;
        }

        if ($carry_over ne '') {
            $text =~ s/ +/ /g; # compact whitespace
            $carry_over .= $text;
            next unless $carry_over =~ m|^  Using \[(.*)\]$|;
            print STDERR (join($blank, $lineno, $carry_over), $endl, $endl) if $trace; # extra line break
            $text = $carry_over;
            $carry_over = '';
        }
# --

        ## Directory [Cedar10.1]<AIS>
        if ($text =~ m|^Directory \[(.*)\]<(.*)>$|) {
            my ($ifs_host, $path) = ($1, $2);

            print STDERR (join($blank, $lineno, 'dir', $ifs_host, $path), $endl) if $trace;
            ## build a 'dir' object here
            my %o;
            $o{flavor} = 'dir';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;

            # path table
            $ifs_index{$ifs_host} = $path;

            my $oref = \%o;
            push @sections, $oref;
            $pending_section = $oref;

            next;
        }

        ## ReadOnly [ifs-server]<ubi>x>
        if ($text =~ m|^ReadOnly \[(.*)\]<(.*)>$|) {
            my ($ifs_host, $path) = ($1, $2);

            print STDERR (join($blank, $lineno, 'readonly', $ifs_host, $path), $endl) if $trace;
            ## build a 'readonly' object here
            my %o;
            $o{flavor} = 'readonly';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;

            # path table
            $ifs_index{$ifs_host} = $path;

            my $oref = \%o;
            push @sections, $oref;
            $pending_section = $oref;

            next;
        }

        ## Exports Imports [Cedar10.1]<Top>CedarDoc.df Of ~=
        if ($text =~ m|^Exports Imports \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($ifs_host, $path, $module, $when) = ($1, $2, $3, $4);

            print STDERR (join($blank, $lineno, 'relay', $ifs_host, $path, $when), $endl) if $trace;
            ## build a 'relay' object here
            my %o;
            $o{flavor} = 'relay';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $o{module} = $module;
            $o{when} = $when;

            # path table
            $ifs_index{$ifs_host} = $path;

            # project graph
            $project_graph{$module} = $active_df;

            my $oref = \%o;
            push @sections, $oref;
            $pending_section = $oref;

# FIXME : add $module to backlog queue
## backlog : 'relay', $oref;
print STDERR (join($blank, 'BACKLOG', $lineno, 'relay', $ifs_host, $path, $when), $endl);

            next;
        }

        ## Include [Cedar10.1]<Top>CedarDoc.df Of ~=
        if ($text =~ m|^Include \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($ifs_host, $path, $module, $when) = ($1, $2, $3, $4);

            print STDERR (join($blank, $lineno, 'include', $ifs_host, $path, $module, $when), $endl) if $trace;
            ## build a 'include' object here
            my %o;
            $o{flavor} = 'include';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $o{module} = $module;
            $o{when} = $when;

            # path table
            $ifs_index{$ifs_host} = $path;

            # project graph
            $project_graph{$module} = $active_df;

            my $oref = \%o;
            push @sections, $oref;
            $pending_section = $oref;

# FIXME : add $module to backlog queue
## backlog : 'include', $oref;
print STDERR (join($blank, 'BACKLOG', $lineno, 'include', $ifs_host, $path, $module, $when), $endl);

            next;
        }

        ## Exports [Cedar10.1]<ApproxSymTab>
        if ($text =~ m|^Exports \[(.*)\]<(.*)>$|) {
            my ($ifs_host, $path) = ($1, $2);
            print STDERR (join($blank, $lineno, 'export', $ifs_host, $path), $endl) if $trace;
            ## build a 'export' object here
            my %o;
            $o{flavor} = 'export';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;

            # path table
            $ifs_index{$ifs_host} = $path;

            my $oref = \%o;
            push @sections, $oref;
            $pending_section = $oref;

            next;
        }

        ## Imports [Cedar10.1]<Top>AIS.df Of >
        if ($text =~ m|^Imports \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($ifs_host, $path, $module, $when) = ($1, $2, $3, $4);
            print STDERR (join($blank, $lineno, 'import', $ifs_host, $path, $module, $when), $endl) if $trace;

            ## build an 'import' object here
            my %o;
            $o{flavor} = 'import';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $o{module} = $module;
            $o{when} = $when;

            # path table
            $ifs_index{$ifs_host} = $path;

            # project graph
            $project_graph{$module} = $active_df;

            my $oref = \%o;
            push @sections, $oref;
            $pending_section = $oref;

# FIXME : add $module to backlog queue
## backlog : 'include', $oref;
print STDERR (join($blank, 'BACKLOG', $lineno, 'import', $ifs_host, $path, $module, $when), $endl);

            next;
        }

        ##  Using [+xr>BasicTypes.h, +xr>Errno.h, ... ]
        if ($text =~ m|^  Using \[(.*)\]$|) {
            my ($item_list) = ($1);

            my @things = split(', ', $item_list);
            # print STDERR (join($blank, $lineno, 'restrict', $item_list), $endl) if $trace;
            print STDERR (join($blank, $lineno, 'restrict', @things), $endl) if $trace;

            ## build a 'restrict' object here
            my %o;
            $o{flavor} = 'restrict';
            $o{lineno} = $lineno;
            $o{things} = \@things;
## FIXME : would it be ok to have multiple 'Using' clauses ??

            my $oref = \%o;
            $pending_section->{restrict} = $oref;
my $module;

            $o{using} = [];
## unclear if name parsing belongs here??
            foreach my $one (@things) {
                my $fo = parse_vname($one);
                push @{ $o{files}}, $fo;

                # add to file table
                my $lname = join($dot, $fo->{base}, $fo->{ext});
                $locals{$lname} = $fo;
                my $module = $pending_section->{module};
                $fo->{owner} = $module;
## FIXME : this strongly binds the meaning of a 'using' after a <section>
            }

            next;
        }

        ## +AbortLockTest.mesa!1                         23-May-91 15:21:58 PDT
        if ($text =~ m|^\s*(\S*) \s*(\S.*) (\S.*) (\S.*)$|) {
            my ($vname, $cal, $time, $tz) = ($1, $2, $3, $4);
            # print STDERR (join($blank, $lineno, $pending_section->{flavor}, 'item', $vname, $cal, $time, $tz), $endl) if $trace;
            my $when = join($blank, $cal, $time, $tz_map{$tz});

            # build an 'entity' item here:
            my %o;
            $o{flavor} = 'entity';
            $o{lineno} = $lineno;
            $o{vname} = $vname;
            $o{when} = $when;

            my $t = Time::Piece->strptime($when, $basictime_fmt);
            my $epoch = $t->epoch; # note this is unix epoch, not a BasicTime
            my $basic_time = $epoch + $bt_adjust;
            print STDERR (join($blank, $lineno, $pending_section->{flavor}, 'item', $vname, $basic_time, $epoch, $when), $endl) if $trace;
            $o{epoch} = $epoch;
            $o{basic_time} = $basic_time;

            my $fo = parse_vname($vname);
            $o{file} = $fo;

            # add to file table
            my $lname = join($dot, $fo->{base}, $fo->{ext});
            $locals{$lname} = $fo;
            $fo->{owner} = $active_df;

## FIXME : donno how 'readonly' plays here
            my $pflav = $pending_section->{flavor}; # dir or readonly ??
            print STDERR (join($blank, $lineno, 'readonly', $lname), $endl) if ($pflav eq 'readonly');

## FIXME : is this the best way to organize the tree?
            ## ensure a list exists :
            $pending_section->{elist} //= [];
            push @{ $pending_section->{elist} }, \%o;

            next if $pending_section->{flavor} eq 'dir';
            next if $pending_section->{flavor} eq 'export';
        }

# --

print STDERR (join($blank, $lineno, 'bj zz'), $endl); ## bj


## FIXME - some syntax I don't know about :
## or is malformed in some way ??

        # how did this get through ??
        if (($pending_section->{flavor} eq 'readonly') and (length($text) == 47) and ($text =~ m|^  (\S*)|)) {
            my $vname = $1;
            print STDERR (join($blank, $lineno, $pending_section->{flavor}, 'raw', $dquot.$vname.$dquot, length($text)), $endl); # if $trace;
            next;
        }

        print STDERR (join($blank, $lineno, $pending_section->{flavor}, $dquot.$text.$dquot, length($text)), $endl); # if $unknown;
    }

    print STDERR (join(' ', $pathname, 'lines:', $#lines+1), $endl) if $debug or $trace;

    $tree{sections} = \@sections;
    my $o = \%tree;
    $r->{tree} = $o; # do this at the end
    return $r;
}

my $densejson = 0;
sub genout {
    my ($adf) = @_;

    my $module = $adf->{goid};
    # for convenience:
    $adf->{ifs_table} = \%ifs_index;
    $adf->{pgraph} = \%project_graph;
    $adf->{locals} = \%locals;
    my $doclet = encode_json($adf);

    # my $cmd = 'python3 -mjson.tool';
    my $cmd = 'jq -S .';
    $cmd = 'cat' if $densejson;
    my $openspec = '|'.$cmd.' > '.$module.'.adf';
    print STDERR (join($blank, 'generating:', $openspec), $endl); # if $debug;
    open(FD, $openspec) or die $cmd.': '.$!;
    print FD $doclet;
    close(FD);
}

foreach my $opt (@ARGV) {
    # --debug
    # --host:Cedar10.1=${XeroxCedar}/release
    if ($opt eq '--debug') { $debug = 1; next };
    if ($opt eq '--trace') { $trace = 1; next };
    if ($opt eq '--unknown') { $unknown = 1; next };
    if ($opt eq '--densejson') { $densejson = 1; next };
    if ($opt =~ /--host:(.*)=(.*)/) {
        my ($a, $b) = ($1, $2);
        $host{$a} = $b;
        print STDERR (join($blank, 'host:', $a, '=>', $b), $endl) if $debug;
        next;
    }

    my $adf = parse_df($opt);

    genout($adf);
}

exit 0;

# --

my $notes = <<'_eof_';

    my %user_cache;
    sub fetch_user {
        my $id = shift;
        $user_cache{$id} //= create_user($id);
        return $user_cache{$id};
    }

    my @comps =  split(/[\[\]<>\/!]+/, $t1);
    @comps = grep { $_ ne '' } @comps; # remove empty (back-to-back delim)

_eof_
