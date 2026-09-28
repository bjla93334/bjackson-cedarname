#!/usr/bin/perl -w
# parse-df.pl ${XeroxCedar}/release/Top/*.df
# parse-df.pl --debug --host:Cedar10.1=${XeroxCedar}/release /Cedar10.1/Top/Interpress.df

use strict;
use warnings;

use Time::Piece;

use lib $ENV{HOME}.'/bin/perlib';
use io::text qw(inhale_file);

my $endl = "\n";
my $dquot = '"';
my $squot = "'";
my $blank = ' ';
my $slash = '/';

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

# my $sepr = '.!>+-$_~?';
my $vname_sepr = '.!>+';

# IsDelim(c) ((c) == '[' || (c) == ']' || (c) == '<' || (c) == '>' || (c) == '/')

# +sub>base.ext!version
sub parse_vname {
    my ($vname) = @_;
    my $xxx = $vname; $xxx =~ s|^\+||;
    my @comps = split(/[\[\]<>\/!]+/, $xxx);
    @comps = grep { $_ ne '' } @comps; # remove empty (back-to-back delim)
    my $parts = join('/', @comps); # ref
    ## print STDERR (join($blank, $parts, @comps, 'vname', $vname), $endl) if $debug;
    return $parts;
}

my %smodel; # map from DF name to abstract object

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
        # $canon = ...; # [host]<path>xx.df!xx@mtime - path may include '>'

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
    print STDERR (join(' ', $pathname, 'lines:', $#lines), $endl) if $debug or $trace;

    my %tree;
    ## $r->{tree} = \%tree; # do this at the end
    $r->{body} = \@lines;

    # active context:
    my $section;
    my $lineno = 0; # primarily for trace/debug

    # tree has a list of 'sections':
    my @sections;
    foreach my $text (@lines) {
        chomp($text);
        $lineno++;
        next if $text =~ m|^\s*$|; # blank lines
        next if $text =~ m|^\s*--|; # comment lines

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
            $section = \%o;
            push @sections, $section;

            next;
        }

        ## ReadOnly [project]<ubi>x>
        if ($text =~ m|^ReadOnly \[(.*)\]<(.*)>$|) {
            my ($ifs_host, $path) = ($1, $2);

            print STDERR (join($blank, $lineno, 'readonly', $ifs_host, $path), $endl) if $trace;
            ## build a 'readonly' object here
            my %o;
            $o{flavor} = 'readonly';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $section = \%o;
            push @sections, $section;

            next;
        }

        ## Exports Imports [Cedar10.1]<Top>CedarDoc.df Of ~=
        if ($text =~ m|^Exports Imports \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($ifs_host, $path, $when) = ($1, $2, $3);

            print STDERR (join($blank, $lineno, 'relay', $ifs_host, $path, $when), $endl) if $trace;
            ## build a 'relay' object here
            my %o;
            $o{flavor} = 'relay';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $o{when} = $when;
            $section = \%o;
            push @sections, $section;
# FIXME : add to backlog queue

            next;
        }

        ## Include [Cedar10.1]<Top>CedarDoc.df Of ~=
        if ($text =~ m|^Include \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($ifs_host, $path, $when) = ($1, $2, $3);

            print STDERR (join($blank, $lineno, 'include', $ifs_host, $path, $when), $endl) if $trace;
            ## build a 'include' object here
            my %o;
            $o{flavor} = 'include';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $o{when} = $when;
            $section = \%o;
            push @sections, $section;
# FIXME : add to backlog queue

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
            $section = \%o;
            push @sections, $section;

            next;
        }

        ## Imports [Cedar10.1]<Top>AIS.df Of >
        if ($text =~ m|^Imports \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($ifs_host, $path, $when) = ($1, $2, $3);
            print STDERR (join($blank, $lineno, 'import', $ifs_host, $path, $when), $endl) if $trace;
            ## build a 'import' object here
            my %o;
            $o{flavor} = 'import';
            $o{lineno} = $lineno;
            $o{ifs_host} = $ifs_host;
            $o{path} = $path;
            $o{when} = $when;
            $section = \%o;
            push @sections, $section;
# FIXME : add to backlog queue

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
            $section->{restrict} = \%o;

## unclear if name parsing belongs here??
            foreach my $xxx (@things) {
                parse_vname($xxx);
            }

            next;
        }

        ## +AbortLockTest.mesa!1                         23-May-91 15:21:58 PDT
        if ($text =~ m|^\s*(\S*) \s*(\S.*) (\S.*) (\S.*)$|) {
            my ($vname, $cal, $time, $tz) = ($1, $2, $3, $4);
            # print STDERR (join($blank, $lineno, $section->{flavor}, 'item', $vname, $cal, $time, $tz), $endl) if $trace;
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
            print STDERR (join($blank, $lineno, $section->{flavor}, 'item', $vname, $basic_time, $epoch, $when), $endl) if $trace;
            $o{epoch} = $epoch;
            $o{basic_time} = $basic_time;

## FIXME : is this the best way to organize the tree?
            ## ensure a list exists :
            $section->{elist} //= [];
            push @{ $section->{elist} }, \%o;


## FIXME : parse name here, or later ??
my $parts = parse_vname($vname);

            next if $section->{flavor} eq 'dir';
            next if $section->{flavor} eq 'export';
        }

## FIXME - some syntax I don't know about :
## or is malformed in some way ??

        # how did this get through ??
        if (($section->{flavor} eq 'readonly') and (length($text) == 47) and ($text =~ m|^  (\S*)|)) {
            my $vname = $1;
            print STDERR (join($blank, $lineno, $section->{flavor}, 'raw', $dquot.$vname.$dquot, length($text)), $endl); # if $trace;
            next;
        }

        print STDERR (join($blank, $lineno, $section->{flavor}, $dquot.$text.$dquot, length($text)), $endl); # if $unknown;
    }

    $tree{sections} = \@sections;
    my $o = \%tree;
    $r->{tree} = $o; # do this at the end
    return $r;
}

my $densejson = 0;
sub genout {
    my ($adf) = @_;
    my $project = $adf->{goid};
    my $doclet = encode_json($adf);
    my $cmd = 'python3 -mjson.tool';
    $cmd = 'cat' if $densejson;
    my $openspec = '|'.$cmd.'>'.$project.'.adf';
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

_eof_
