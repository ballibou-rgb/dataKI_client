# dataKI client — ProGuard/R8 rules.
# Keep the core model classes (populated from JSON once the API lands in M4).
-keep class ovh.datanet.dataki.client.core.** { *; }
