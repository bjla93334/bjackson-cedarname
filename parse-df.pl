#!/usr/bin/perl -w
# parse-df.pl ${XeroxCedar}/release/Top/*.df

use strict;
use warnings;

use Time::Piece;

use lib $ENV{HOME}.'/bin/perlib';
use io::text qw(inhale_file);

my $endl = "\n";
my $dquot = '"';
my $squot = "'";
my $blank = ' ';

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

if ($#ARGV < 0) {
    die(join($blank, 'usage:',
        '[-verbose]',
        'file.df[.gz] ...'
    ));
}

my $cedar = $ENV{XeroxCedar};

my %freq;

# my $sepr = '.!>+-$_~?';
my $vname_sepr = '.!>+';

# +sub>base.ext!version
sub parse_vname {
    my ($vname) = @_;
    # print STDERR (join($blank, 'vname', $vname), $endl);
    my $parts = ''; # ref
    return $parts;
}

sub process {
    my ($filename) = @_;
    my @lines = inhale_file($filename);
    # print STDERR (join(' ', $filename, 'lines', $#lines), $endl);
    # ${XeroxCedar}/release/VersionMapBuilder

    my $section = '';
    my $lineno = 0;
    foreach my $text (@lines) {
        chomp($text);
        $lineno++;
        next if $text =~ m|^\s*$|; # blank lines
        next if $text =~ m|^\s*--|; # comment lines

        ## Directory [Cedar10.1]<AIS>
        if ($text =~ m|^Directory \[(.*)\]<(.*)>$|) {
            my ($pme, $path) = ($1, $2);
            $section = 'dir';
            # print STDERR (join($blank, $lineno, 'dir', $pme, $path), $endl);
            next;
        }

        ## ReadOnly [project]<ubi>x>
        if ($text =~ m|^ReadOnly \[(.*)\]<(.*)>$|) {
            my ($pme, $path) = ($1, $2);
            $section = 'readonly';
            # print STDERR (join($blank, $lineno, 'readonly', $pme, $path), $endl);
            next;
        }

        ## Exports Imports [Cedar10.1]<Top>CedarDoc.df Of ~=
        if ($text =~ m|^Exports Imports \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($pme, $path, $when) = ($1, $2, $3);
            $section = 'relay';
            # print STDERR (join($blank, $lineno, 'relay', $pme, $path, $when), $endl);
            next;
        }

        ## Include [Cedar10.1]<Top>CedarDoc.df Of ~=
        if ($text =~ m|^Include \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($pme, $path, $when) = ($1, $2, $3);
            $section = 'include';
            # print STDERR (join($blank, $lineno, 'include', $pme, $path, $when), $endl);
            next;
        }

        ## Exports [Cedar10.1]<ApproxSymTab>
        if ($text =~ m|^Exports \[(.*)\]<(.*)>$|) {
            my ($pme, $path) = ($1, $2);
            $section = 'export';
            # print STDERR (join($blank, $lineno, 'export', $pme, $path), $endl);
            next;
        }

        ## Imports [Cedar10.1]<Top>AIS.df Of >
        if ($text =~ m|^Imports \[(.*)\]<(.*)>(.*)\.df Of (.*)$|) {
            my ($pme, $path, $when) = ($1, $2, $3);
            $section = 'import';
            # print STDERR (join($blank, $lineno, 'import', $pme, $path, $when), $endl);
            next;
        }

        ##  Using [+xr>BasicTypes.h, +xr>Errno.h, ... ]
        if ($text =~ m|^  Using \[(.*)\]$|) {
            my ($item_list) = ($1);

            my @things = split(', ', $item_list);
            $section = 'restrict';
            # print STDERR (join($blank, $lineno, 'restrict', $item_list), $endl);
            # print STDERR (join($blank, $lineno, 'restrict', @things), $endl);

## FIXME
            foreach my $xxx (@things) {
                parse_vname($xxx);
            }

            next;
        }

        ## +AbortLockTest.mesa!1                         23-May-91 15:21:58 PDT
        if ($text =~ m|^\s*(\S*) \s*(\S.*) (\S.*) (\S.*)$|) {
            my ($vname, $cal, $time, $tz) = ($1, $2, $3, $4);
            # print STDERR (join($blank, $lineno, $section, 'item', $vname, $cal, $time, $tz), $endl);

            my $cal_spec = join($blank, $2, $3, $tz_map{$4});
            my $t = Time::Piece->strptime($cal_spec, $basictime_fmt);
            my $epoch = $t->epoch;
            # print STDERR (join($blank, $lineno, $section, 'item', $vname, $epoch, $cal_spec), $endl);

my $parts = parse_vname($vname);
## FIXME
            next if $section eq 'dir';
            next if $section eq 'export';
        }

        # how did this get through ??
        if (($section eq 'readonly') and (length($text) == 47) and ($text =~ m|^  (\S*)|)) {
            my $vname = $1;
            # print STDERR (join($blank, $lineno, $section, 'raw', $dquot.$vname.$dquot, length($text)), $endl);
            next;
        }

        print STDERR (join(' ', $filename, 'lines', $#lines), $endl);
        print STDERR (join($blank, $lineno, $section, $dquot.$text.$dquot, length($text)), $endl);
    }
}

foreach my $opt (@ARGV) {
    process($opt);
    # my $ref = process($opt);
}

exit 0;

# --

my $notes = <<'_eof_';

_eof_
