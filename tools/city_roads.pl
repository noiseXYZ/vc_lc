# Builds scriptfiles/GPS_VC.dat and GPS_LC.dat (road networks of the Vice City and
# Liberty City maps, vc2sa4samp / lc2sa4samp) in the GPS.dat format:
#   0 x y z -1 id      node
#   1 from to 0        connection (written once in each direction, like GPS.dat)
#
# From the original games' car paths (in 1/16 units):
#   Vice City: data/maps/paths.ipl, groups "1, -1" (car) of 12 nodes with absolute positions
#   GTA III:   "car, model, name" groups in the map IDEs, relative to every IPL instance of
#              that model (heading = -2 * atan2(qz, qw), III stores the rotation inverted)
# Node: type (1 external, 2 internal, 0 unused), next (index in the group, -1 none), x, y, z.
# Externals of different groups on the same spot are joined, like the games do.
#
#   perl city_roads.pl vc <paths.ipl> <out.dat>
#   perl city_roads.pl lc <out.dat> <ide or ipl files...>
#
# The original files: https://z.noclip.website/GrandTheftAutoViceCity/data/maps/paths.ipl and
# GrandTheftAuto3/data/maps/<area>/<area>.ide / .ipl (see the area list in noclip's
# src/GrandTheftAuto3/scenes.json).

use strict;
use warnings;

my %city = (
    vc => {base => 32768, dx => 6000.0,  dy => 1050.0, dz => -5.5},
    lc => {base => 40960, dx => -5350.0, dy => -200.0, dz => 0.0},
);
my $join = 1.0;    # externals closer than this (metres) are one node (the games use 1.0)

my $which = shift or die "usage: city_roads.pl vc|lc ...\n";
my $c     = $city{$which} or die "vc or lc\n";

my @groups;        # [ [type, next, x, y, z], ... x12 ]

sub lines_of {
    my $path = shift;
    open my $f, '<', $path or die "$path: $!";
    my @l = map { s/\r?\n$//r } <$f>;
    close $f;
    return @l;
}

my $out;
if ($which eq 'vc') {
    my $in;
    ($in, $out) = @ARGV;
    my $car = 0;
    my $g;
    for (lines_of($in)) {
        if (/^(\d+),\s*-?\d+\s*$/) { $car = $1 == 1; $g = []; push @groups, $g if $car; next; }
        next unless $car && /^\s+(\d+),\s*(-?\d+),\s*-?\d+,\s*(\S+),\s*(\S+),\s*(\S+),/;
        push @$g, [$1, $2, $3 / 16, $4 / 16, $5 / 16];
    }
} else {
    $out = shift @ARGV;
    my (%paths, @inst);
    for my $file (@ARGV) {
        my $section = '';
        my $g;
        for (lines_of($file)) {
            s/#.*//;
            next unless /\S/;
            if (/^\s*(inst|path|objs|tobj|end|cull|zone|pick|2dfx|hier|cars|peds|weap)\s*$/) { $section = $1; next; }
            if ($section eq 'path') {
                if (/^\s*(car|ped)\s*,\s*(\d+)/) { $g = $1 eq 'car' ? ($paths{$2} = []) : undef; next; }
                next unless $g && /^\s*(\d+),\s*(-?\d+),\s*-?\d+,\s*(\S+),\s*(\S+),\s*(\S+),/;
                push @$g, [$1, $2, $3 / 16, $4 / 16, $5 / 16];
            } elsif ($section eq 'inst') {
                my @v = split /\s*,\s*/;
                push @inst, [@v[0, 2, 3, 4, 10, 11]] if @v >= 12;    # id, x, y, z, qz, qw
            }
        }
    }
    for my $i (@inst) {
        my ($id, $x, $y, $z, $qz, $qw) = @$i;
        my $p = $paths{$id} or next;
        my $h = -2 * atan2($qz, $qw);
        my ($cs, $sn) = (cos($h), sin($h));
        push @groups, [map { [$_->[0], $_->[1], $x + $_->[2] * $cs - $_->[3] * $sn, $y + $_->[2] * $sn + $_->[3] * $cs, $z + $_->[4]] } @$p];
    }
}

# nodes: every used node of every group; externals on one spot share an id
my (@nodes, %links, %cell);
my $cellsize = 4;
sub node_id {
    my ($x, $y, $z, $external) = @_;
    if ($external) {
        my ($cx, $cy) = (int($x / $cellsize), int($y / $cellsize));
        for my $ox (-1 .. 1) {
            for my $oy (-1 .. 1) {
                for my $id (@{$cell{$cx + $ox, $cy + $oy} // []}) {
                    my $n = $nodes[$id];
                    return $id if abs($n->[0] - $x) < $join && abs($n->[1] - $y) < $join && abs($n->[2] - $z) < 3;
                }
            }
        }
    }
    push @nodes, [$x, $y, $z];
    my $id = $#nodes;
    push @{$cell{int($x / $cellsize), int($y / $cellsize)}}, $id if $external;
    return $id;
}

for my $g (@groups) {
    my @ids = map { $_->[0] ? node_id($_->[2], $_->[3], $_->[4], $_->[0] == 1) : -1 } @$g;
    for my $i (0 .. $#$g) {
        my $next = $g->[$i][1];
        next if $ids[$i] < 0 || $next < 0 || $next > $#$g || $ids[$next] < 0 || $ids[$i] == $ids[$next];
        my ($a, $b) = sort { $a <=> $b } $ids[$i], $ids[$next];
        $links{"$a $b"} = 1;
    }
}

# keep only pieces of at least $min_piece nodes: drops the VC stadium's race track (indoors)
# and stray fragments, keeps the golf course and the beach
my $min_piece = 100;
my %adj;
for (keys %links) {
    my ($a, $b) = split;
    push @{$adj{$a}}, $b;
    push @{$adj{$b}}, $a;
}
my (%keep, %seen);
my $dropped = 0;
for my $start (0 .. $#nodes) {
    next if $seen{$start};
    my @piece = ($start);
    $seen{$start} = 1;
    for (my $i = 0; $i < @piece; $i++) {
        for (@{$adj{$piece[$i]} // []}) {
            next if $seen{$_}++;
            push @piece, $_;
        }
    }
    if (@piece >= $min_piece) { $keep{$_} = 1 for @piece } else { $dropped += @piece }
}

open my $o, '>', $out or die "$out: $!";
binmode $o, ':crlf';
for (grep { $keep{$_} } 0 .. $#nodes) {
    printf $o "0 %f %f %f -1 %d\n", $nodes[$_][0] + $c->{dx}, $nodes[$_][1] + $c->{dy}, $nodes[$_][2] + $c->{dz}, $c->{base} + $_;
}
my $kept_links = 0;
for (sort keys %links) {
    my ($a, $b) = split;
    next unless $keep{$a};
    printf $o "1 %d %d 0\n", $c->{base} + $a, $c->{base} + $b;
    printf $o "1 %d %d 0\n", $c->{base} + $b, $c->{base} + $a;
    $kept_links++;
}
close $o;
printf "%s: %d groups, %d nodes (%d in small pieces dropped), %d connections -> %s\n", $which, scalar @groups, scalar keys %keep, $dropped, $kept_links, $out;
