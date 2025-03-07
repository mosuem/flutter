import 'package:flutter/material.dart';

void main() {
  runApp(const MainApp());
}

class MainApp extends StatelessWidget {
  const MainApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const MaterialApp(
      home: Scaffold(body: Center(child: Icon(TwoIcons.acUnit))),
    );
  }
}

@staticIconProvider
sealed class TwoIcons {
  static const IconData acUnit = IconData(0xe800, fontFamily: 'TwoIcons');
  static const IconData whatshot = IconData(0xe801, fontFamily: 'TwoIcons');
}
