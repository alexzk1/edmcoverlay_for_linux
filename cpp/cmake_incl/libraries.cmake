# Assorted libraries we need.

CPMAddPackage(
  URI "gh:nlohmann/json@3.12.0"
  OPTIONS "JSON_BuildTests OFF"
)
CPMAddPackage(
  URI "gh:sammycage/lunasvg@3.5.0"
  OPTIONS "USE_SYSTEM_PLUTOVG OFF"
)

