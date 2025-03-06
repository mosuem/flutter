// Copyright (c) 2024, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'package:native_assets_cli/native_assets_cli_builder.dart';

/// Extension to the [HookConfig] providing access to configuration specific
/// to code assets (only available if code assets are supported).
extension FlutterHookConfig on HookConfig {
  /// Code asset specific configuration.
  FlutterConfig get flutter => FlutterConfig.fromJson(json);
}

/// Configuration for hook writers if code assets are supported.
class FlutterConfig {
  factory FlutterConfig.fromJson(Map<String, Object?> json) {
    return FlutterConfig._(
      fontSubsetBinary: json.getNested(<String>[_configKey, _flutterKey, _fontSubsetBinaryKey]),
    );
  }
  // Should not be made public, class will be replaced as a view on `json`.
  FlutterConfig._({String? fontSubsetBinary}) : _fontSubsetBinary = fontSubsetBinary;

  final String? _fontSubsetBinary;

  String? get fontSubsetBinary => _fontSubsetBinary;
}

/// Extension to initialize code specific configuration on link/build inputs.
extension CodeAssetBuildInputBuilder on HookConfigBuilder {
  void setupFlutter({String? fontSubsetBinary}) {
    json.setNested(<String>[_configKey, _flutterKey, _fontSubsetBinaryKey], fontSubsetBinary);
  }
}

const String _configKey = 'config';
const String _flutterKey = 'flutter';
const String _fontSubsetBinaryKey = 'fontSubsetBinary';

extension MapJsonUtils on Map<String, Object?> {
  void setNested(List<String> nestedMapKeys, Object? value) {
    Map<String, Object?> map = this;
    for (final String key in nestedMapKeys.sublist(0, nestedMapKeys.length - 1)) {
      map = (map[key] ??= <String, Object?>{}) as Map<String, Object?>;
    }
    map[nestedMapKeys.last] = value;
  }

  T? getNested<T extends Object>(List<String> nestedMapKeys) {
    Map<String, Object?> map = this;
    for (final String key in nestedMapKeys.sublist(0, nestedMapKeys.length - 1)) {
      map = (map[key] ??= <String, Object?>{}) as Map<String, Object?>;
    }
    return map[nestedMapKeys.last] as T?;
  }
}
