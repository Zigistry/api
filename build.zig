const std = @import("std");

const SOURCE_FILES = [_][]const u8{
    "./src/main.cpp",
    "./src/search.cpp",
    "./src/scroll.cpp",
};

// FLAGS = -Linclude -lsql -std=c++23

const FLAGS = [_][]const u8{
    "-std=c++23",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const root_module = b.createModule(.{
        // .root_source_file = b.path("src/main.cpp"),
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    root_module.addCSourceFiles(.{
        .files = &SOURCE_FILES,
        .flags = &FLAGS,
    });

    root_module.addIncludePath(b.path("./include"));
    root_module.addIncludePath(.{
        .cwd_relative = "/opt/homebrew/opt/asio/include",
    });

    root_module.addObjectFile(b.path("./include/libsql.dylib"));

    const exe = b.addExecutable(.{
        .name = "app",
        .root_module = root_module,
    });

    b.installArtifact(exe);
}
