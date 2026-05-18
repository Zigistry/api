const std = @import("std");

const SOURCE_FILES = [_][]const u8{
    "./src/main.cpp",
    "./src/search.cpp",
    "./src/scroll.cpp",
    "./src/helper_lib/helper_lib.cpp",
};

const FLAGS = [_][]const u8{
    "-std=c++23",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const root_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    root_module.addCSourceFiles(.{
        .files = &SOURCE_FILES,
        .flags = &FLAGS,
        .language = .cpp,
    });

    root_module.addIncludePath(b.path("./include"));
    root_module.addLibraryPath(b.path("./include"));
    root_module.linkSystemLibrary("sql", .{});

    const exe = b.addExecutable(.{
        .name = "app",
        .root_module = root_module,
    });

    b.installArtifact(exe);
}
