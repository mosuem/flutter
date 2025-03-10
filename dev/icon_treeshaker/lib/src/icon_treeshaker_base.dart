// TODO: Put public facing types in this file.

import 'dart:convert' show utf8;
import 'dart:io' show Process;

import 'package:file/file.dart' show File, FileSystem;
import 'package:logging/logging.dart' show Logger;
import 'package:mime/mime.dart' as mime;

/// Checks if you are awesome. Spoiler: you are.
class Awesome {
  bool get isAwesome => true;
}

/// The MIME types for supported font sets.
const Set<String> kTtfMimeTypes = <String>{
  'font/ttf', // based on internet search
  'font/opentype',
  'font/otf',
  'application/x-font-opentype',
  'application/x-font-otf',
  'application/x-font-ttf', // based on running locally.
};

/// Calls font-subset, which transforms the [fontFile] font file to a
/// subsetted version at [outputPath].
///
/// If [enabled] is false, or the relative path is not recognized as an icon
/// font used in the Flutter application, this returns false.
/// If the font-subset subprocess fails, it will [throwToolExit].
/// Otherwise, it will return true.
Future<bool> subsetFont({
  required File fontFile,
  required String outputPath,
  required IconTreeShakerData iconData,
  required FileSystem fs,
  required String pathToSubsetBinary,
  required Logger logger,
}) async {
  if (fontFile.lengthSync() < 12) {
    return false;
  }
  final String? mimeType = mime.lookupMimeType(
    fontFile.path,
    headerBytes: await fontFile.openRead(0, 12).first,
  );
  if (!kTtfMimeTypes.contains(mimeType)) {
    return false;
  }

  final File fontSubset = fs.file(pathToSubsetBinary);

  if (!fontSubset.existsSync()) {
    throw IconTreeShakerException._(
      'The font-subset utility is missing. Run "flutter doctor".',
    );
  }

  final List<String> args = <String>[outputPath, fontFile.path];
  final Iterable<String> requiredCodePointStrings = iconData.codePoints.map(
    (int codePoint) => codePoint.toString(),
  );
  final Iterable<String> optionalCodePointStrings = iconData.optionalCodePoints
      .map((int codePoint) => 'optional:$codePoint');
  final String codePointsString = requiredCodePointStrings
      .followedBy(optionalCodePointStrings)
      .join(' ');
  logger.info(
    'Running font-subset: ${args.join(' ')}, '
    'using codepoints $codePointsString',
  );
  final Process fontSubsetProcess = await Process.start(fontSubset.path, args);
  try {
    fontSubsetProcess.stdin.write(codePointsString);
    await fontSubsetProcess.stdin.flush();
    await fontSubsetProcess.stdin.close();
  } on Exception {
    // handled by checking the exit code.
  }

  final int code = await fontSubsetProcess.exitCode;
  if (code != 0) {
    logger.severe(await utf8.decodeStream(fontSubsetProcess.stdout));
    logger.severe(await utf8.decodeStream(fontSubsetProcess.stderr));
    throw IconTreeShakerException._(
      'Font subsetting failed with exit code $code.',
    );
  }
  logger.info(getSubsetSummaryMessage(fontFile, fs.file(outputPath)));
  return true;
}

/// The font family name, relative path to font file, and list of code points
/// the application is using.
class IconTreeShakerData {
  /// All parameters are required.
  const IconTreeShakerData({
    required this.family,
    required this.codePoints,
    required this.optionalCodePoints,
  });

  /// The font family name, e.g. "MaterialIcons".
  final String family;

  /// The list of code points for the font.
  final List<int> codePoints;

  /// The list of code points to be optionally added, if they exist in the
  /// input font. Otherwise, the tool will silently omit them.
  final List<int> optionalCodePoints;

  @override
  String toString() => 'FontSubsetData($family, $codePoints)';
}

class IconTreeShakerException implements Exception {
  IconTreeShakerException._(this.message);

  final String message;

  @override
  String toString() =>
      'IconTreeShakerException: $message\n\n'
      'To disable icon tree shaking, pass --no-tree-shake-icons to the requested '
      'flutter build command';
}

String getSubsetSummaryMessage(File inputFont, File outputFont) {
  final String fontName = inputFont.basename;
  final double inputSize = inputFont.lengthSync().toDouble();
  final double outputSize = outputFont.lengthSync().toDouble();
  final double reductionBytes = inputSize - outputSize;
  final String reductionPercentage = (reductionBytes / inputSize * 100)
      .toStringAsFixed(1);
  return 'Font asset "$fontName" was tree-shaken, reducing it from '
      '${inputSize.ceil()} to ${outputSize.ceil()} bytes '
      '($reductionPercentage% reduction). Tree-shaking can be disabled '
      'by providing the --no-tree-shake-icons flag when building your app.';
}
