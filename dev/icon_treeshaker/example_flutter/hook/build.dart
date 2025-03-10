import 'package:icon_treeshaker/hook_helper.dart';
import 'package:native_assets_cli/native_assets_cli.dart';

void main(List<String> arguments) {
  build(arguments, (input, output) async {
    output.assets.font.add(
      FontAsset(
        family: 'TwoIcons',
        file: input.packageRoot.resolve('fonts/TwoIcons.ttf'),
        package: input.packageName,
      ),
    );
  });
}
