# Builds maps/vc.png / maps/lc.png from the original radar tiles: radar00.txd .. radar63.txd
# out of the game's models/gta3.img (8 x 8 tiles of 128 px, tile 0 = north-west corner, the
# whole image = original world -2000..2000).
#
#   perl radar2png.pl <folder with radarNN.txd> <out.png>
#
# Vice City and San Andreas tiles are DXT1. GTA III tiles keep the map in the alpha channel (one tint, roads
# and water see-through), so they are drawn over BG_R/BG_G/BG_B (lc.png: 28 36 48). CLEAN_Y
# copies that row over the rows above it (lc.png: 62, paints out the placeholder town names
# on the unused mainland).
#
# lc.png: BG_R=28 BG_G=36 BG_B=48 CLEAN_Y=62 perl radar2png.pl lc lc.png
# vc.png: perl radar2png.pl vc vc.png
# sa.png: TILES=12 perl radar2png.pl sa sa.png   (radar00..radar143 out of San Andreas' gta3.img)
# Then regenerate the client's map data: perl tools/bin2hpp.pl > client/src/map_data.hpp

use strict;
use warnings;
use Compress::Zlib;

my ($dir, $out) = @ARGV;
die "usage: perl radar2png.pl <dir> <out.png>\n" unless defined $out;

my $N   = $ENV{TILES} // 8;    # tiles per side (San Andreas: TILES=12)
my $T   = 128;
my $W   = $N * $T;
my $buf = "\0" x ($W * $W * 4);
my @bg  = map { $ENV{$_} // 0 } qw(BG_R BG_G BG_B);

sub c565 {
    my $c = shift;
    my ($r, $g, $b) = (($c >> 11) & 31, ($c >> 5) & 63, $c & 31);
    return (($r << 3) | ($r >> 2), ($g << 2) | ($g >> 4), ($b << 3) | ($b >> 2));
}

for my $i (0 .. $N * $N - 1) {
    my $f = sprintf('%s/radar%02d.txd', $dir, $i);
    open my $fh, '<:raw', $f or die "$f: $!";
    my $d = do { local $/; <$fh> };
    close $fh;

    # TexDictionary > Struct (skipped) > TextureNative > Struct
    my $p = 12;
    my (undef, $sz) = unpack('VV', substr($d, $p, 8));
    $p += 12 + $sz + 12;
    my $s = $p + 12;
    my ($rf, $fourcc, $w, $h, undef, undef, undef, $cmp) = unpack('Va4vvCCCC', substr($d, $s + 72, 16));
    $cmp = 1 if $fourcc eq 'DXT1';    # San Andreas (D3D9 natives): flags byte + FourCC
    die "$f: ${w}x$h\n" unless $w == $T && $h == $T;
    my $len = unpack('V', substr($d, $s + 88, 4));
    my $px  = substr($d, $s + 92, $len);

    my ($tx, $ty) = (($i % $N) * $T, int($i / $N) * $T);
    my $put = sub {
        my ($x, $y, @rgb) = @_;
        substr($buf, (($ty + $y) * $W + $tx + $x) * 4, 4) = pack('C4', @rgb, 255);
    };

    if (($rf & 0xF00) == 0x500 && $cmp == 0) {    # B8G8R8A8, blended over the background
        for my $y (0 .. $T - 1) {
            for my $x (0 .. $T - 1) {
                my ($b, $g, $r, $a) = unpack('C4', substr($px, ($y * $T + $x) * 4, 4));
                my @c = ($r, $g, $b);
                $put->($x, $y, map { int(($c[$_] * $a + $bg[$_] * (255 - $a)) / 255) } 0 .. 2);
            }
        }
    } elsif ($cmp == 1) {                          # DXT1
        my $o = 0;
        for my $by (0 .. $T / 4 - 1) {
            for my $bx (0 .. $T / 4 - 1) {
                my ($c0, $c1, $bits) = unpack('vvV', substr($px, $o, 8));
                $o += 8;
                my @a   = c565($c0);
                my @b   = c565($c1);
                my @pal = ([@a], [@b]);
                if ($c0 > $c1) {
                    push @pal, [map { int((2 * $a[$_] + $b[$_]) / 3) } 0 .. 2], [map { int(($a[$_] + 2 * $b[$_]) / 3) } 0 .. 2];
                } else {
                    push @pal, [map { int(($a[$_] + $b[$_]) / 2) } 0 .. 2], [0, 0, 0];
                }
                for my $k (0 .. 15) {
                    $put->($bx * 4 + $k % 4, $by * 4 + int($k / 4), @{$pal[($bits >> (2 * $k)) & 3]});
                }
            }
        }
    } else {
        die "$f: raster format $rf, compression $cmp\n";
    }
}

if (my $cy = $ENV{CLEAN_Y}) {
    substr($buf, $_ * $W * 4, $W * 4) = substr($buf, $cy * $W * 4, $W * 4) for 0 .. $cy - 1;
}

# PNG, 8-bit RGB
my $raw = '';
for my $y (0 .. $W - 1) {
    $raw .= "\0";
    $raw .= substr($buf, ($y * $W + $_) * 4, 3) for 0 .. $W - 1;
}
sub chunk { my ($t, $c) = @_; return pack('N', length $c) . $t . $c . pack('N', crc32($t . $c)); }
open my $o, '>:raw', $out or die "$out: $!";
print $o "\x89PNG\r\n\x1a\n", chunk('IHDR', pack('NNCCCCC', $W, $W, 8, 2, 0, 0, 0)), chunk('IDAT', compress($raw, 9)), chunk('IEND', '');
close $o;
