import 'dart:convert' show jsonDecode;
import 'dart:io' as io;

import 'package:collection/collection.dart';
import 'package:file/file.dart';
import 'package:file/local.dart';
import 'package:icon_treeshaker/hook_helper.dart';
import 'package:icon_treeshaker/icon_treeshaker.dart';
import 'package:logging/logging.dart';
import 'package:native_assets_cli/data_assets.dart';
import 'package:native_assets_cli/src/config.dart';
import 'package:path/path.dart' as p;
import 'package:record_use/record_use.dart' as record_use;

Future<void> main(List<String> arguments) async => await link(arguments, (
  input,
  output,
) async {
  final logger = Logger('link');
  final fontAssets = input.assets.fonts.toList();
  FileSystem fs = LocalFileSystem();
  var usages = input.usages;
  //TODO(mosum): Get path to subset from flutter
  final pathToSubsetBinary = '';
  logger.info('Getting tree-shaker data for icons.');
  final Map<FontAsset, IconTreeShakerData> pathToTreeshakeData = getIconData(
    usages,
    fontAssets,
  );
  for (final fontAsset in fontAssets) {
    logger.info('Checking if $fontAsset should be tree-shaken.');
    final IconTreeShakerData? iconData = pathToTreeshakeData[fontAsset];
    if (iconData == null) {
      logger.info('No IconData usages of $fontAsset found.');
      continue;
    }
    final fontFile = fs.file(fontAsset.file);
    logger.info('Running $pathToSubsetBinary on $fontFile.');
    final success = await subsetFont(
      fontFile: fontFile,
      outputPath: p.join(
        input.outputDirectoryShared.path,
        //TODO(mosum): Handle duplicates
        p.basename(fontAsset.file),
      ),
      iconData: iconData,
      fs: fs,
      pathToSubsetBinary: pathToSubsetBinary,
      logger: logger,
    );
    if (!success) {
      throw io.ProcessException(
        pathToSubsetBinary,
        [],
        'Failure when trying to treeshake $fontAsset. Check the log for more details.',
      );
    }
    logger.info('Success, shrunk $fontFile!.');
  }
});

/// The key is the family, or might be prefixed
/// [result[familyKey] = asset;](https://github.com/flutter/flutter/blob/08b1bdecd6a632e7b30901efaaf19d0f759bd0f5/packages/flutter_tools/lib/src/build_system/targets/icon_tree_shaker.dart#L278)
///
/// The [ids] should
Map<FontAsset, IconTreeShakerData> getIconData(
  record_use.RecordedUsages usages,
  List<FontAsset> fonts,
) {
  final groupedInstances = (usages.instancesOf(
            record_use.Identifier(
              importUri: 'package:flutter/widgets.dart',
              name: 'IconData',
            ),
          ) ??
          [])
      .map((e) => e.instanceConstant)
      .map((e) => e.fields)
      .map(
        (e) => (
          codePoint: (e['codePoint'] as record_use.IntConstant).value,
          family: (e['fontFamily'] as record_use.StringConstant?)?.value,
          fontPackage: (e['fontPackage'] as record_use.StringConstant?)?.value,
          matchTextDirection:
              (e['matchTextDirection'] as record_use.StringConstant?)?.value,
        ),
      )
      .groupListsBy(
        (instance) => (family: instance.family, package: instance.fontPackage),
      );
  final map = <FontAsset, IconTreeShakerData>{};
  for (final MapEntry(key: key, value: instances) in groupedInstances.entries) {
    var font = fonts.firstWhereOrNull(
      (font) => font.family == key.family && font.package == key.package,
    );
    if (font != null) {
      // Add space as an optional code point, as web uses it to measure the font height.
      //TODO(mosum): add web as OS, and pass it to the data config (TBC)
      bool isWeb = 1 == 0;
      const int kSpacePoint = 32;
      final List<int> optionalCodePoints = isWeb ? <int>[kSpacePoint] : <int>[];
      map[font] = IconTreeShakerData(
        family: font.family,
        relativePath: font.file,
        codePoints: instances.map((instance) => instance.codePoint).toList(),
        optionalCodePoints: optionalCodePoints,
      );
    }
  }

  // As in https://github.com/flutter/flutter/blob/08b1bdecd6a632e7b30901efaaf19d0f759bd0f5/packages/flutter_tools/lib/src/build_system/targets/icon_tree_shaker.dart#L358
  // Use 'packages/$package/$family' as keys

  return map;
}

extension on LinkInput {
  record_use.RecordedUsages get usages {
    final usagesFile = recordedUsagesFile;
    final usagesContent = io.File.fromUri(usagesFile!).readAsStringSync();
    final usagesJson = jsonDecode(usagesContent) as Map<String, dynamic>;
    return record_use.RecordedUsages.fromJson(usagesJson);
  }
}

extension on LinkInputAssets {
  Iterable<FontAsset> get fonts => encodedAssets
      .where((e) => e.type == FontAsset.type)
      .map(FontAsset.fromEncoded);
}
