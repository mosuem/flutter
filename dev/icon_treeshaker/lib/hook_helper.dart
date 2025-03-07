// Copyright (c) 2025, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'dart:collection';

import 'package:native_assets_cli/data_assets.dart';
import 'package:native_assets_cli/native_assets_cli.dart';

/// Data bundled with a Dart or Flutter application.
///
/// A data asset is accessible in a Dart or Flutter application. To retrieve an
/// asset at runtime, the [id] is used. This enables access to the asset
/// irrespective of how and where the application is run.
///
/// An data asset must provide a [DataAsset.file]. The Dart and Flutter SDK will
/// bundle this code in the final application.
final class FontAsset {
  final String family;

  /// https://api.flutter.dev/flutter/dart-ui/FontStyle.html values - italic or normal
  final String? style;

  /// integer multiple of 100, between 100 and 900
  final int? weight;

  /// relative path in the package
  final String file;

  /// The package which contains this asset.
  final String package;

  /// The identifier for this data asset.
  /// TODO(mosum): don't use family to allow multiple fonts per family
  String get id => 'package:$package/$family';

  /// Constructs a [DataAsset] from an [EncodedAsset].
  factory FontAsset.fromEncoded(EncodedAsset asset) {
    assert(asset.type == DataAsset.type);
    final jsonMap = asset.encoding;
    return FontAsset(
      family: jsonMap[_familyKey] as String,
      style: jsonMap[_styleKey] as String?,
      weight: jsonMap[_weightKey] as int?,
      package: jsonMap[_packageKey] as String,
      file: jsonMap[_fileKey] as String,
    );
  }

  @override
  bool operator ==(Object other) {
    if (other is! FontAsset) {
      return false;
    }
    return other.package == package &&
        other.file == file &&
        other.family == family &&
        other.style == style &&
        other.weight == weight;
  }

  @override
  int get hashCode => Object.hash(package, file, family, style, weight);

  EncodedAsset encode() => EncodedAsset(
    FontAsset.type,
    SplayTreeMap<String, Object>.from({
      _packageKey: package,
      _fileKey: file,
      _familyKey: family,
      _styleKey: style,
      _weightKey: weight,
    }),
  );

  @override
  String toString() => 'FontAsset(${encode().encoding})';

  static const String type = 'icon';

  FontAsset({
    required this.family,
    required this.style,
    required this.weight,
    required this.file,
    required this.package,
  });

  FontAsset copyWith({
    String? family,
    String? style,
    int? weight,
    String? file,
    String? package,
  }) => FontAsset(
    family: family ?? this.family,
    style: style ?? this.style,
    weight: weight ?? this.weight,
    file: file ?? this.file,
    package: package ?? this.package,
  );
}

const _familyKey = 'family';
const _styleKey = 'style';
const _weightKey = 'weight';
const _packageKey = 'package';
const _fileKey = 'file';

/// Extension to the [LinkOutputBuilder] providing access to emitting data
/// assets (only available if data assets are supported).
extension FontAssetLinkOutputBuilder on EncodedAssetLinkOutputBuilder {
  /// Provides access to emitting data assets.
  FontAssetLinkOutputBuilderAdd get font => FontAssetLinkOutputBuilderAdd(this);
}

/// Extension on [LinkOutputBuilder] to emit data assets.
extension type FontAssetLinkOutputBuilderAdd(
  EncodedAssetLinkOutputBuilder _output
) {
  /// Adds the given [asset] to the link hook output.
  void add(FontAsset asset) => _output.addEncodedAsset(asset.encode());

  /// Adds the given [assets] to the link hook output.
  void addAll(Iterable<FontAsset> assets) => assets.forEach(add);
}

/// Extension to the [LinkOutputBuilder] providing access to emitting data
/// assets (only available if data assets are supported).
extension FontAssetBuildOutputBuilder on EncodedAssetBuildOutputBuilder {
  /// Provides access to emitting data assets.
  FontAssetBuildOutputBuilderAdd get font =>
      FontAssetBuildOutputBuilderAdd(this);
}

/// Extension on [LinkOutputBuilder] to emit data assets.
extension type FontAssetBuildOutputBuilderAdd(
  EncodedAssetBuildOutputBuilder _output
) {
  /// Adds the given [asset] to the link hook output.
  void add(FontAsset asset) =>
      _output.addEncodedAsset(asset.encode(), linkInPackage: 'icon_treeshaker');

  /// Adds the given [assets] to the link hook output.
  void addAll(Iterable<FontAsset> assets) => assets.forEach(add);
}
