| 支持的目标 | ESP32-S3 |
| --------- | -------- |

# _示例项目_

（有关示例的更多信息，请参阅上级 'examples' 目录中的 README.md 文件。

这是最简单的可构建示例。该示例由命令使用，该命令将项目复制到用户指定的路径并设置其名称。有关更多信息，请访问文档页面[docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)`idf.py create-project`

## 如何使用示例
我们鼓励用户使用该示例作为新项目的模板。 推荐的方法是按照 [docs 页面](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)的说明进行。

## 示例文件夹内容

项目 **sample_project** 包含一个 C 语言 [main.c](main/main.c) 的源文件。该文件位于文件夹 [main](main) 中。

ESP-IDF 项目是使用 CMake 构建的。项目生成配置包含在文件中，这些文件提供一组描述项目的源文件和目标的指令和说明 （可执行文件、库或两者）。`CMakeLists.txt`

以下是 project 文件夹中剩余文件的简短说明。

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```
此外，示例项目还包含 Makefile 和 component.mk 文件，用于基于 Make 的旧版构建系统。 使用 CMake 和 idf.py 进行构建时，不使用也不需要它们。