#!/usr/bin/env python3
"""Guided interface for preparing TreeFileBuilder projects."""
from __future__ import annotations

import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import traceback
from pathlib import Path
from tkinter import (
    BOTH,
    BOTTOM,
    END,
    LEFT,
    RIGHT,
    X,
    Y,
    BooleanVar,
    Listbox,
    Menu,
    StringVar,
    TclError,
    Text,
    Tk,
    filedialog,
    messagebox,
    simpledialog,
    ttk,
)

from typing import Any, Callable, Sequence

from swg_tool.preflight import (
    estimate_tree_size,
    format_preflight_report,
    run_tree_preflight,
)
from swg_tool.response import (
    ResponseFileBuilder,
    ResponseFileBuilderError,
    ResponseFileEntry,
)
from swg_tool.treebuilder import TreeBuildResult, TreeFileBuilder, TreeFileBuilderError
from swg_tool.treefile_ops import (
    TreeFileExtractorError,
    TreeFileListEntry,
    extract_treefile,
    list_treefile,
    resolve_treefile_extractor,
    treefile_requires_passphrase,
)

_TOOLS_DIR = Path(__file__).resolve().parents[2]
_SWG_TOOL_DIR = _TOOLS_DIR / "swg-tool"

DEFAULT_EXECUTABLE_NAMES = [
    "TreeFileBuilder",
    "TreeFileBuilder.exe",
    str(_SWG_TOOL_DIR / "TreeFileBuilder.exe"),
    str(_TOOLS_DIR / "TreeFileBuilder.exe"),
]

DEFAULT_PAGE_SIZE = 1000
PAGE_SIZE_OPTIONS = (250, 500, 1000, 2000, 5000)


class TreeFileBuilderGui:
    """Minimal workflow for building .tre/.tres/.tresx files."""

    def __init__(self, master: Tk) -> None:
        self.master = master
        master.title("TreeFile Builder")
        master.minsize(720, 520)

        self._configure_style()

        self.builder_path = StringVar(value=self._find_default_executable())
        self.source_folder = StringVar()
        self.output_directory = StringVar()
        self.base_name = StringVar()
        self.passphrase = StringVar()
        self.build_tre = BooleanVar(value=True)
        self.build_tres = BooleanVar(value=False)
        self.build_tresx = BooleanVar(value=False)

        self.debug_builder_path = StringVar(value="Builder not resolved yet.")
        self.debug_capabilities = StringVar(value="Capabilities not loaded.")

        self._task_queue: queue.Queue[tuple[str, str, Any, str | None]] = queue.Queue()
        self._active_task: str | None = None
        self._debug_refresh_active = False
        self._busy_widgets: list[ttk.Widget] = []

        self.update_paths: list[Path] = []
        self._source_trace_after: int | None = None
        self._builder_trace_after: int | None = None
        self._treefile_tabs: dict[str, dict[str, Any]] = {}
        self._treefile_tabs_by_path: dict[Path, str] = {}
        self._treefile_last_dir: Path | None = None
        self._treefile_passphrases: dict[Path, str] = {}
        self._treefile_task_context: dict[str, Any] | None = None
        self._content_entries: list[ResponseFileEntry] = []
        self._content_page = 1
        self._content_page_size = DEFAULT_PAGE_SIZE
        self._content_page_count = 1
        self._content_index_active = False
        self._content_source_path: Path | None = None

        self._build_layout()
        self._build_menu()
        self._set_status("Select a content folder to begin.")
        self._refresh_content_index()
        self._refresh_debug_info()
        self._stop_progress()
        self._set_busy_ui(False)
        self.master.after(150, self._poll_async_queue)

        self.builder_path.trace_add("write", self._on_builder_path_changed)

    # ------------------------------------------------------------------
    # UI construction helpers
    # ------------------------------------------------------------------
    def _configure_style(self) -> None:
        style = ttk.Style()
        theme = style.theme_use()
        if theme == "classic":
            try:
                style.theme_use("default")
            except Exception:  # pragma: no cover - depends on platform
                pass

    def _build_layout(self) -> None:
        container = ttk.Frame(self.master, padding=12)
        container.pack(fill=BOTH, expand=True)

        description = (
            "Configure the TreeFileBuilder executable, select a source folder,"
            " optionally add update folders, then generate .tre, .tres, or .tresx files."
        )
        ttk.Label(
            container,
            text=description,
            wraplength=680,
            justify="left",
        ).pack(anchor="w", pady=(0, 12))

        builder_frame = ttk.LabelFrame(container, text="TreeFileBuilder executable")
        builder_frame.pack(fill=X)

        builder_row = ttk.Frame(builder_frame)
        builder_row.pack(fill=X, padx=8, pady=8)
        ttk.Entry(builder_row, textvariable=self.builder_path).pack(
            side=LEFT, fill=X, expand=True
        )
        ttk.Button(builder_row, text="Browse", command=self._browse_builder).pack(
            side=LEFT, padx=(8, 0)
        )

        content_frame = ttk.LabelFrame(container, text="Content folders")
        content_frame.pack(fill=BOTH, expand=True, pady=12)

        self._add_labeled_entry(
            content_frame,
            "Primary source",
            self.source_folder,
            self._browse_source,
        )
        self.source_folder.trace_add("write", self._on_source_changed)

        updates_header = ttk.Frame(content_frame)
        updates_header.pack(fill=X, padx=8, pady=(12, 4))
        ttk.Label(
            updates_header,
            text="Update folders (optional)",
        ).pack(side=LEFT)

        updates_toolbar = ttk.Frame(content_frame)
        updates_toolbar.pack(fill=X, padx=8)
        ttk.Button(
            updates_toolbar, text="Add folder", command=self._add_update_folder
        ).pack(side=LEFT)
        ttk.Button(
            updates_toolbar,
            text="Remove selected",
            command=self._remove_selected_updates,
        ).pack(side=LEFT, padx=(8, 0))
        ttk.Button(
            updates_toolbar, text="Clear", command=self._clear_updates
        ).pack(side=LEFT, padx=(8, 0))

        list_container = ttk.Frame(content_frame)
        list_container.pack(fill=X, padx=8, pady=(4, 8))
        self.updates_listbox = Listbox(list_container, height=5, selectmode="extended")
        self.updates_listbox.pack(side=LEFT, fill=BOTH, expand=True)
        scroll_y = ttk.Scrollbar(list_container, orient="vertical")
        scroll_y.pack(side=RIGHT, fill=Y)
        self.updates_listbox.configure(yscrollcommand=scroll_y.set)
        scroll_y.configure(command=self.updates_listbox.yview)

        self._content_notebook = ttk.Notebook(content_frame)
        self._content_notebook.pack(fill=BOTH, expand=True, padx=8, pady=(0, 8))
        self._content_notebook.bind(
            "<<NotebookTabChanged>>", self._on_content_tab_changed
        )

        self._content_index_tab = ttk.Frame(self._content_notebook)
        self._content_notebook.add(self._content_index_tab, text="Indexed contents")

        tree_container = ttk.Frame(self._content_index_tab)
        tree_container.pack(fill=BOTH, expand=True, padx=4, pady=4)

        paging_toolbar = ttk.Frame(self._content_index_tab)
        paging_toolbar.pack(fill=X, padx=4, pady=(0, 4))
        ttk.Label(paging_toolbar, text="Page size").pack(side=LEFT)
        self.page_size_var = StringVar(value=str(self._content_page_size))
        self.page_size_combo = ttk.Combobox(
            paging_toolbar,
            textvariable=self.page_size_var,
            values=[str(value) for value in PAGE_SIZE_OPTIONS],
            width=6,
            state="readonly",
        )
        self.page_size_combo.pack(side=LEFT, padx=(6, 12))
        self.page_size_combo.bind("<<ComboboxSelected>>", self._on_page_size_changed)
        self.page_prev_button = ttk.Button(
            paging_toolbar, text="Previous", command=self._show_previous_page
        )
        self.page_prev_button.pack(side=LEFT)
        self.page_next_button = ttk.Button(
            paging_toolbar, text="Next", command=self._show_next_page
        )
        self.page_next_button.pack(side=LEFT, padx=(6, 0))
        self.page_expand_button = ttk.Button(
            paging_toolbar, text="Expand page", command=self._expand_current_page
        )
        self.page_expand_button.pack(side=LEFT, padx=(12, 0))
        self.page_status = StringVar(value="Page 0 of 0")
        ttk.Label(paging_toolbar, textvariable=self.page_status).pack(
            side=RIGHT
        )

        columns = ("type", "origin")
        self.content_tree = ttk.Treeview(
            tree_container,
            columns=columns,
            show="tree headings",
            selectmode="extended",
        )
        self.content_tree.heading("#0", text="Path")
        self.content_tree.heading("type", text="Type")
        self.content_tree.heading("origin", text="Source")
        self.content_tree.column("#0", stretch=True, width=360)
        self.content_tree.column("type", width=90, anchor="center")
        self.content_tree.column("origin", width=180, stretch=False)
        tree_scroll_y = ttk.Scrollbar(tree_container, orient="vertical")
        tree_scroll_y.pack(side=RIGHT, fill=Y)
        tree_scroll_x = ttk.Scrollbar(tree_container, orient="horizontal")
        tree_scroll_x.pack(side=BOTTOM, fill=X)

        self.content_tree.configure(
            yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set
        )
        tree_scroll_y.configure(command=self.content_tree.yview)
        tree_scroll_x.configure(command=self.content_tree.xview)
        self.content_tree.pack(side=LEFT, fill=BOTH, expand=True)

        summary_bar = ttk.Frame(self._content_index_tab)
        summary_bar.pack(fill=X, padx=4, pady=(0, 4))
        self.content_summary = StringVar(
            value="Select a source folder to index its contents."
        )
        ttk.Label(summary_bar, textvariable=self.content_summary).pack(anchor="w")

        output_frame = ttk.LabelFrame(container, text="Output")
        output_frame.pack(fill=X)

        self._add_labeled_entry(
            output_frame,
            "Output directory",
            self.output_directory,
            self._browse_output_directory,
        )

        name_row = ttk.Frame(output_frame)
        name_row.pack(fill=X, padx=8, pady=(4, 4))
        ttk.Label(name_row, text="Base file name").pack(side=LEFT)
        ttk.Entry(name_row, textvariable=self.base_name, width=32).pack(
            side=LEFT, padx=(12, 0)
        )

        options_row = ttk.Frame(output_frame)
        options_row.pack(fill=X, padx=8, pady=(4, 8))
        ttk.Checkbutton(
            options_row, text="Build .tre", variable=self.build_tre
        ).pack(side=LEFT)
        ttk.Checkbutton(
            options_row, text="Build .tres (encrypted)", variable=self.build_tres
        ).pack(side=LEFT, padx=(12, 0))
        ttk.Checkbutton(
            options_row, text="Build .tresx (encrypted)", variable=self.build_tresx
        ).pack(side=LEFT, padx=(12, 0))
        ttk.Label(options_row, text="Passphrase (optional)").pack(side=LEFT, padx=(24, 0))
        ttk.Entry(options_row, textvariable=self.passphrase, show="*", width=20).pack(
            side=LEFT, padx=(6, 0)
        )

        actions_row = ttk.Frame(container)
        actions_row.pack(fill=X, pady=(12, 12))

        self.quick_tres_button = ttk.Button(
            actions_row,
            text="Quick .tres/.tresx",
            command=self._quick_create_tres,
        )
        self.quick_tres_button.pack(side=RIGHT)

        self.generate_button = ttk.Button(
            actions_row,
            text="Generate tree files",
            command=self._generate_tree_files,
        )
        self.generate_button.pack(side=RIGHT, padx=(0, 8))

        self._busy_widgets.extend([self.quick_tres_button, self.generate_button])

        log_frame = ttk.LabelFrame(container, text="Diagnostics")
        log_frame.pack(fill=BOTH, expand=True)

        self.log_notebook = ttk.Notebook(log_frame)
        self.log_notebook.pack(fill=BOTH, expand=True, padx=8, pady=8)

        self.log_tab = ttk.Frame(self.log_notebook)
        self.debug_tab = ttk.Frame(self.log_notebook)
        self.log_notebook.add(self.log_tab, text="Activity log")
        self.log_notebook.add(self.debug_tab, text="Debugging")

        log_toolbar = ttk.Frame(self.log_tab)
        log_toolbar.pack(fill=X, padx=4, pady=(4, 0))
        ttk.Button(log_toolbar, text="Clear", command=self._clear_log).pack(side=RIGHT)
        ttk.Button(
            log_toolbar, text="Copy", command=self._copy_log_to_clipboard
        ).pack(side=RIGHT, padx=(0, 8))

        log_container = ttk.Frame(self.log_tab)
        log_container.pack(fill=BOTH, expand=True, padx=4, pady=4)
        self.log_widget = Text(log_container, wrap="none", height=12)
        self.log_widget.pack(side=LEFT, fill=BOTH, expand=True)
        log_scroll = ttk.Scrollbar(log_container, orient="vertical")
        log_scroll.pack(side=RIGHT, fill=Y)
        self.log_widget.configure(yscrollcommand=log_scroll.set)
        log_scroll.configure(command=self.log_widget.yview)

        debug_toolbar = ttk.Frame(self.debug_tab)
        debug_toolbar.pack(fill=X, padx=4, pady=(4, 0))
        self.debug_refresh_button = ttk.Button(
            debug_toolbar, text="Refresh info", command=self._refresh_debug_info
        )
        self.debug_refresh_button.pack(side=LEFT)
        self.debug_help_button = ttk.Button(
            debug_toolbar, text="Run --help", command=self._run_builder_help
        )
        self.debug_help_button.pack(side=LEFT, padx=(8, 0))
        self._busy_widgets.extend([self.debug_refresh_button, self.debug_help_button])

        info_frame = ttk.LabelFrame(self.debug_tab, text="Builder details")
        info_frame.pack(fill=X, padx=4, pady=(4, 4))
        ttk.Label(info_frame, text="Executable:").pack(anchor="w", padx=8, pady=(4, 0))
        ttk.Label(
            info_frame,
            textvariable=self.debug_builder_path,
            wraplength=620,
        ).pack(fill=X, padx=8, pady=(0, 4))
        ttk.Label(info_frame, text="Capabilities:").pack(anchor="w", padx=8)
        ttk.Label(
            info_frame,
            textvariable=self.debug_capabilities,
            wraplength=620,
        ).pack(fill=X, padx=8, pady=(0, 8))

        debug_output_frame = ttk.LabelFrame(self.debug_tab, text="Builder output")
        debug_output_frame.pack(fill=BOTH, expand=True, padx=4, pady=(0, 4))
        debug_output_container = ttk.Frame(debug_output_frame)
        debug_output_container.pack(fill=BOTH, expand=True, padx=4, pady=4)
        self.debug_output = Text(
            debug_output_container, wrap="none", height=10, state="disabled"
        )
        self.debug_output.pack(side=LEFT, fill=BOTH, expand=True)
        debug_scroll = ttk.Scrollbar(debug_output_container, orient="vertical")
        debug_scroll.pack(side=RIGHT, fill=Y)
        self.debug_output.configure(yscrollcommand=debug_scroll.set)
        debug_scroll.configure(command=self.debug_output.yview)
        self._write_debug_output(
            "Press \"Run --help\" to capture diagnostic output from TreeFileBuilder."
        )

        status_frame = ttk.Frame(container)
        status_frame.pack(fill=X, pady=(12, 0))
        ttk.Separator(status_frame, orient="horizontal").pack(fill=X, pady=(0, 6))
        self.progress = ttk.Progressbar(status_frame, mode="indeterminate")
        self.progress.pack(fill=X, pady=(0, 6))
        self.status_label = ttk.Label(status_frame, text="Ready")
        self.status_label.pack(anchor="w")

    def _build_menu(self) -> None:
        menubar = Menu(self.master)
        treefile_menu = Menu(menubar, tearoff=0)
        treefile_menu.add_command(
            label="Open Tree File", command=self._open_treefile
        )
        treefile_menu.add_command(
            label="Extract selected", command=self._extract_selected_treefile_entries
        )
        treefile_menu.add_separator()
        treefile_menu.add_command(label="Exit", command=self.master.destroy)
        menubar.add_cascade(label="Tree File", menu=treefile_menu)

        create_menu = Menu(menubar, tearoff=0)
        create_menu.add_command(
            label="Quick .tres/.tresx", command=self._quick_create_tres
        )
        create_menu.add_command(
            label="Smart .tres/.tresx (preflight)",
            command=lambda: self._quick_create_tres_smart(use_gpu=False),
        )
        create_menu.add_command(
            label="Smart .tres/.tresx (GPU preflight)",
            command=lambda: self._quick_create_tres_smart(use_gpu=True),
        )
        create_menu.add_separator()
        create_menu.add_command(
            label="Create .tres/.tresx from current sources",
            command=lambda: self._create_tres_from_current_sources(
                run_preflight=False, use_gpu=False
            ),
        )
        create_menu.add_command(
            label="Smart .tres/.tresx from current sources",
            command=lambda: self._create_tres_from_current_sources(
                run_preflight=True, use_gpu=False
            ),
        )
        create_menu.add_command(
            label="Smart .tres/.tresx from current sources (GPU)",
            command=lambda: self._create_tres_from_current_sources(
                run_preflight=True, use_gpu=True
            ),
        )
        create_menu.add_separator()
        create_menu.add_command(label="Exit", command=self.master.destroy)
        menubar.add_cascade(label="Create Tree File", menu=create_menu)
        self.master.config(menu=menubar)

    def _add_labeled_entry(
        self,
        parent: ttk.Widget,
        label: str,
        variable: StringVar,
        callback,
    ) -> None:
        row = ttk.Frame(parent)
        row.pack(fill=X, padx=8, pady=(8, 0))
        ttk.Label(row, text=label).pack(side=LEFT)
        entry = ttk.Entry(row, textvariable=variable)
        entry.pack(side=LEFT, fill=X, expand=True, padx=(12, 0))
        ttk.Button(row, text="Browse", command=callback).pack(side=LEFT, padx=(8, 0))

    # ------------------------------------------------------------------
    # Browse helpers
    # ------------------------------------------------------------------
    def _browse_builder(self) -> None:
        initial = self.builder_path.get().strip()
        directory = Path(initial).parent if initial else Path.cwd()
        selected = filedialog.askopenfilename(
            title="Select TreeFileBuilder executable",
            initialdir=str(directory),
        )
        if selected:
            self.builder_path.set(selected)

    def _browse_source(self) -> None:
        selected = filedialog.askdirectory(title="Select source folder")
        if selected:
            path = Path(selected)
            self.source_folder.set(selected)
            if not self.output_directory.get().strip():
                self.output_directory.set(str(path.parent))
            if not self.base_name.get().strip():
                self.base_name.set(path.name)
            self._refresh_content_index()

    def _browse_output_directory(self) -> None:
        selected = filedialog.askdirectory(title="Select output directory")
        if selected:
            self.output_directory.set(selected)

    # ------------------------------------------------------------------
    # Update folder management
    # ------------------------------------------------------------------
    def _add_update_folder(self) -> None:
        selected = filedialog.askdirectory(title="Add update folder")
        if not selected:
            return
        path = Path(selected)
        if not path.exists():
            messagebox.showerror("Update folder", f"Folder not found: {path}")
            return
        if path in self.update_paths:
            return
        self.update_paths.append(path)
        self._refresh_updates_list()
        self._refresh_content_index()

    def _remove_selected_updates(self) -> None:
        selections = list(self.updates_listbox.curselection())
        if not selections:
            return
        for index in reversed(selections):
            try:
                del self.update_paths[index]
            except IndexError:
                continue
        self._refresh_updates_list()
        self._refresh_content_index()

    def _clear_updates(self) -> None:
        self.update_paths.clear()
        self._refresh_updates_list()
        self._refresh_content_index()

    def _refresh_updates_list(self) -> None:
        self.updates_listbox.delete(0, END)
        for path in self.update_paths:
            self.updates_listbox.insert(END, str(path))

    # ------------------------------------------------------------------
    # Content indexing
    # ------------------------------------------------------------------
    def _on_source_changed(self, *_args) -> None:
        if self._source_trace_after is not None:
            try:
                self.master.after_cancel(self._source_trace_after)
            except Exception:  # pragma: no cover - defensive cancellation
                pass
        self._source_trace_after = self.master.after(300, self._refresh_content_index)

    def _is_index_tab_active(self) -> bool:
        if not hasattr(self, "_content_notebook"):
            return True
        return self._content_notebook.select() == str(self._content_index_tab)

    def _on_content_tab_changed(self, _event: Any) -> None:
        self._update_page_controls()

    def _clear_content_tree(self) -> None:
        for child in self.content_tree.get_children():
            self.content_tree.delete(child)

    def _update_page_controls(self) -> None:
        has_pages = self._content_page_count > 1
        if not self._is_index_tab_active():
            self.page_prev_button.configure(state="disabled")
            self.page_next_button.configure(state="disabled")
            self.page_expand_button.configure(state="disabled")
            self.page_size_combo.configure(state="disabled")
            self.page_status.set("Tree file view")
            return
        if not self._content_index_active:
            self.page_prev_button.configure(state="disabled")
            self.page_next_button.configure(state="disabled")
            self.page_expand_button.configure(state="disabled")
            self.page_size_combo.configure(state="disabled")
            self.page_status.set("Index not available")
            return

        self.page_prev_button.configure(
            state="normal" if self._content_page > 1 and has_pages else "disabled"
        )
        self.page_next_button.configure(
            state=(
                "normal"
                if self._content_page < self._content_page_count and has_pages
                else "disabled"
            )
        )
        self.page_expand_button.configure(
            state="normal" if self._content_index_active else "disabled"
        )
        self.page_size_combo.configure(
            state="readonly"
        )
        self.page_status.set(
            f"Page {self._content_page} of {self._content_page_count}"
        )

    def _on_page_size_changed(self, _event: Any) -> None:
        try:
            new_size = int(self.page_size_var.get())
        except (TypeError, ValueError):
            return
        if new_size <= 0:
            return
        self._content_page_size = new_size
        self._content_page = 1
        self._render_content_page()

    def _show_previous_page(self) -> None:
        if self._content_page > 1:
            self._content_page -= 1
            self._render_content_page()

    def _show_next_page(self) -> None:
        if self._content_page < self._content_page_count:
            self._content_page += 1
            self._render_content_page()

    def _expand_current_page(self) -> None:
        if not self._content_index_active:
            return

        def expand_item(item_id: str) -> None:
            self.content_tree.item(item_id, open=True)
            for child_id in self.content_tree.get_children(item_id):
                expand_item(child_id)

        for root_id in self.content_tree.get_children():
            expand_item(root_id)

    def _determine_entry_origin(
        self, file_path: Path, source_root: Path, update_roots: list[Path]
    ) -> str:
        try:
            resolved_file = file_path.resolve()
        except OSError:
            resolved_file = file_path

        if resolved_file == source_root or source_root in resolved_file.parents:
            return "Primary source"

        for update_root in update_roots:
            if resolved_file == update_root or update_root in resolved_file.parents:
                return f"Update: {update_root.name}"

        return "Additional source"

    def _refresh_content_index(self) -> None:
        if self._source_trace_after is not None:
            callback_id = self._source_trace_after
            self._source_trace_after = None
            try:
                self.master.after_cancel(callback_id)
            except Exception:  # pragma: no cover - defensive cancellation
                pass

        if not hasattr(self, "content_tree"):
            return

        self._clear_content_tree()
        self._content_entries = []
        self._content_page = 1
        self._content_page_count = 1
        self._content_index_active = False
        self._content_source_path = None
        self._update_page_controls()

        source_text = self.source_folder.get().strip()
        if not source_text:
            self.content_summary.set("Select a source folder to index its contents.")
            return

        source_path = Path(source_text)
        if not source_path.exists() or not source_path.is_dir():
            self.content_summary.set("Source folder is not available.")
            return

        all_sources: list[Path] = [source_path] + self.update_paths

        try:
            builder = ResponseFileBuilder(
                entry_root=source_path, allow_overrides=True
            )
            entries = builder.build_entries(all_sources)
        except ResponseFileBuilderError as exc:
            self.content_summary.set(f"Indexing failed: {exc}")
            return
        except Exception as exc:  # pragma: no cover - unexpected failures
            self.content_summary.set(f"Unexpected indexing error: {exc}")
            return

        self._content_entries = entries
        self._content_source_path = source_path
        self._content_index_active = True
        self._render_content_page()

    def _render_content_page(self) -> None:
        self._clear_content_tree()

        if not self._content_entries:
            self.content_summary.set("No content entries were indexed.")
            self._content_page = 1
            self._content_page_count = 1
            self._update_page_controls()
            return

        total_entries = len(self._content_entries)
        page_size = max(1, self._content_page_size)
        self._content_page_count = max(
            1, (total_entries + page_size - 1) // page_size
        )
        if self._content_page > self._content_page_count:
            self._content_page = self._content_page_count
        if self._content_page < 1:
            self._content_page = 1
        start_index = (self._content_page - 1) * page_size
        end_index = min(start_index + page_size, total_entries)
        displayed_entries = self._content_entries[start_index:end_index]

        if self._content_source_path is None:
            self.content_summary.set("Source folder is not available.")
            self._content_index_active = False
            self._update_page_controls()
            return

        source_root = self._content_source_path.resolve()
        update_roots = [path.resolve() for path in self.update_paths]

        root_label = f"{self._content_source_path.name or self._content_source_path}/"
        root_id = self.content_tree.insert(
            "",
            "end",
            text=root_label,
            values=("Folder", "Primary source"),
            open=True,
        )

        node_lookup: dict[str, str] = {"": root_id}

        for entry in displayed_entries:
            parts = entry.entry.split("/") if entry.entry else []
            parent_key = ""
            for index, part in enumerate(parts):
                key = f"{parent_key}/{part}" if parent_key else part
                is_file = index == len(parts) - 1
                if key not in node_lookup:
                    parent_id = node_lookup[parent_key]
                    label = f"{part}{'' if is_file else '/'}"
                    values = ("File", self._determine_entry_origin(entry.source, source_root, update_roots))
                    if not is_file:
                        values = ("Folder", "")
                    node_id = self.content_tree.insert(
                        parent_id,
                        "end",
                        text=label,
                        values=values,
                        open=False,
                    )
                    node_lookup[key] = node_id
                parent_key = key

        folder_count = 1 + len(self.update_paths)
        folder_label = "folder" if folder_count == 1 else "folders"
        summary = (
            f"Indexed {total_entries} files from {folder_count} {folder_label}. "
            f"Showing page {self._content_page} of {self._content_page_count} "
            f"({page_size} per page)."
        )
        self.content_summary.set(summary)
        self._update_page_controls()

    # ------------------------------------------------------------------
    # Tree file browsing
    # ------------------------------------------------------------------
    def _open_treefile(self) -> None:
        initial_dir = self._treefile_last_dir or Path.cwd()
        filetypes = [
            ("Tree file archives", "*.tre *.tres *.tresx"),
            ("Tree archives (.tre)", "*.tre"),
            ("Encrypted tree archives (.tres)", "*.tres"),
            ("Encrypted tree archives (.tresx)", "*.tresx"),
            ("All files", "*"),
        ]
        selected = filedialog.askopenfilename(
            title="Open tree file",
            initialdir=str(initial_dir),
            filetypes=filetypes,
        )
        if not selected:
            return
        treefile = Path(selected)
        self._treefile_last_dir = treefile.parent
        passphrase = self._prompt_treefile_passphrase(treefile)
        if passphrase is None:
            return

        self.log_notebook.select(self.log_tab)
        self._log(f"Listing tree file: {treefile}\n")

        self._start_treefile_list_task(treefile, passphrase)

    def _prompt_treefile_passphrase(
        self, treefile: Path, *, allow_cached: bool = True
    ) -> str | None:
        if not treefile_requires_passphrase(treefile):
            return ""

        treefile_key = self._normalize_treefile_path(treefile)
        cached = self._treefile_passphrases.get(treefile_key)
        if allow_cached and cached is not None:
            use_cached = messagebox.askyesnocancel(
                "Passphrase",
                f"Use the cached passphrase for {treefile.name}?",
            )
            if use_cached is None:
                return None
            if use_cached:
                return cached

        if allow_cached:
            passphrase = self.passphrase.get()
            if passphrase.strip():
                self._treefile_passphrases[treefile_key] = passphrase
                return passphrase

        prompt_message = "Enter the passphrase for this encrypted tree file (optional)."
        if cached is not None:
            prompt_message = (
                "Enter the passphrase for this encrypted tree file (optional).\n"
                "Leave blank to clear the cached passphrase."
            )
        prompted = simpledialog.askstring(
            "Passphrase",
            prompt_message,
            show="*",
        )
        if prompted is None:
            return None
        passphrase = prompted.rstrip("\r\n")
        if passphrase.strip():
            self._treefile_passphrases[treefile_key] = passphrase
        else:
            passphrase = ""
            self._treefile_passphrases.pop(treefile_key, None)
        return passphrase

    def _create_treefile_tab(self, treefile: Path) -> dict[str, Any]:
        existing_tab = self._treefile_tabs_by_path.get(treefile)
        if existing_tab:
            return self._treefile_tabs[existing_tab]

        tab = ttk.Frame(self._content_notebook)
        self._content_notebook.add(tab, text=treefile.name)

        tree_container = ttk.Frame(tab)
        tree_container.pack(fill=BOTH, expand=True, padx=4, pady=4)
        columns = ("type", "origin")
        treeview = ttk.Treeview(
            tree_container,
            columns=columns,
            show="tree headings",
            selectmode="extended",
        )
        treeview.heading("#0", text="Path")
        treeview.heading("type", text="Type")
        treeview.heading("origin", text="Source")
        treeview.column("#0", stretch=True, width=360)
        treeview.column("type", width=90, anchor="center")
        treeview.column("origin", width=180, stretch=False)
        tree_scroll_y = ttk.Scrollbar(tree_container, orient="vertical")
        tree_scroll_y.pack(side=RIGHT, fill=Y)
        tree_scroll_x = ttk.Scrollbar(tree_container, orient="horizontal")
        tree_scroll_x.pack(side=BOTTOM, fill=X)
        treeview.configure(
            yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set
        )
        tree_scroll_y.configure(command=treeview.yview)
        tree_scroll_x.configure(command=treeview.xview)
        treeview.pack(side=LEFT, fill=BOTH, expand=True)

        summary_bar = ttk.Frame(tab)
        summary_bar.pack(fill=X, padx=4, pady=(0, 4))
        summary_var = StringVar(value="Tree file not loaded yet.")
        ttk.Label(summary_bar, textvariable=summary_var).pack(anchor="w")

        tab_id = str(tab)
        state = {
            "tab_id": tab_id,
            "tab": tab,
            "treefile": treefile,
            "tree": treeview,
            "summary": summary_var,
            "nodes": {},
        }
        self._treefile_tabs[tab_id] = state
        self._treefile_tabs_by_path[treefile] = tab_id
        return state

    def _get_active_treefile_state(self) -> dict[str, Any] | None:
        if not hasattr(self, "_content_notebook"):
            return None
        selected_tab = self._content_notebook.select()
        return self._treefile_tabs.get(selected_tab)

    def _populate_treefile_listing(
        self, treefile: Path, entries: list[TreeFileListEntry]
    ) -> None:
        tab_state = self._create_treefile_tab(treefile)
        treeview: ttk.Treeview = tab_state["tree"]
        nodes: dict[str, dict[str, str | bool]] = tab_state["nodes"]

        for child in treeview.get_children():
            treeview.delete(child)
        nodes.clear()

        root_label = treefile.name
        root_id = treeview.insert(
            "",
            "end",
            text=root_label,
            values=("Tree file", "Tree file"),
            open=True,
        )
        nodes[root_id] = {"path": "", "is_file": False}

        node_lookup: dict[str, str] = {"": root_id}
        for entry in entries:
            normalized_path = entry.path.strip().replace("\\", "/")
            if not normalized_path:
                continue
            parts = normalized_path.split("/")
            parent_key = ""
            for index, part in enumerate(parts):
                key = f"{parent_key}/{part}" if parent_key else part
                is_file = index == len(parts) - 1
                if key not in node_lookup:
                    parent_id = node_lookup[parent_key]
                    label = f"{part}{'' if is_file else '/'}"
                    values = ("File", "Tree file") if is_file else ("Folder", "")
                    node_id = treeview.insert(
                        parent_id,
                        "end",
                        text=label,
                        values=values,
                        open=False,
                    )
                    node_lookup[key] = node_id
                    nodes[node_id] = {
                        "path": key,
                        "is_file": is_file,
                    }
                parent_key = key

        summary = f"Loaded {len(entries)} entries from {treefile.name}."
        tab_state["summary"].set(summary)
        self._content_notebook.select(tab_state["tab"])
        self._update_page_controls()

    def _collect_treefile_entries(
        self,
        nodes: dict[str, dict[str, str | bool]],
        treeview: ttk.Treeview,
        item_id: str,
    ) -> list[str]:
        node_info = nodes.get(item_id)
        if not node_info:
            return []
        if node_info.get("is_file"):
            path = node_info.get("path")
            return [path] if isinstance(path, str) and path else []
        entries: list[str] = []
        for child in treeview.get_children(item_id):
            entries.extend(self._collect_treefile_entries(nodes, treeview, child))
        return entries

    def _extract_selected_treefile_entries(self) -> None:
        tab_state = self._get_active_treefile_state()
        if not tab_state:
            messagebox.showinfo(
                "Extract selected",
                "Select a tree file tab first to extract its contents.",
            )
            return

        treeview: ttk.Treeview = tab_state["tree"]
        nodes: dict[str, dict[str, str | bool]] = tab_state["nodes"]
        selections = treeview.selection()
        if not selections:
            messagebox.showinfo(
                "Extract selected",
                "Select one or more entries to extract.",
            )
            return

        selected_entries: list[str] = []
        for item_id in selections:
            selected_entries.extend(
                self._collect_treefile_entries(nodes, treeview, item_id)
            )

        normalized_entries = sorted({entry for entry in selected_entries if entry})
        if not normalized_entries:
            messagebox.showinfo(
                "Extract selected",
                "No file entries selected for extraction.",
            )
            return

        output_dir = filedialog.askdirectory(
            title="Select output folder for extraction"
        )
        if not output_dir:
            return

        treefile: Path = tab_state["treefile"]
        passphrase = self._prompt_treefile_passphrase(treefile)
        if passphrase is None:
            return

        self.log_notebook.select(self.log_tab)
        self._log(f"Extracting {len(normalized_entries)} entries from {treefile}\n")

        self._start_treefile_extract_task(
            treefile,
            Path(output_dir),
            normalized_entries,
            passphrase,
        )

    def _on_builder_path_changed(self, *_args) -> None:
        if self._builder_trace_after is not None:
            try:
                self.master.after_cancel(self._builder_trace_after)
            except Exception:  # pragma: no cover - defensive cancellation
                pass
        self._builder_trace_after = self.master.after(400, self._refresh_debug_info)

    # ------------------------------------------------------------------
    # Log helpers
    # ------------------------------------------------------------------
    def _log(self, text: str) -> None:
        def append() -> None:
            self.log_widget.insert(END, text)
            self.log_widget.see(END)

        if threading.current_thread() is threading.main_thread():
            append()
        else:
            self.master.after(0, append)

    def _clear_log(self) -> None:
        self.log_widget.delete("1.0", END)

    def _copy_log_to_clipboard(self) -> None:
        content = self.log_widget.get("1.0", END)
        self.master.clipboard_clear()
        self.master.clipboard_append(content)

    def _write_debug_output(self, content: str) -> None:
        def update() -> None:
            self.debug_output.configure(state="normal")
            self.debug_output.delete("1.0", END)
            self.debug_output.insert("1.0", content)
            self.debug_output.configure(state="disabled")
            self.debug_output.see(END)

        if threading.current_thread() is threading.main_thread():
            update()
        else:
            self.master.after(0, update)

    def _set_status(
        self, message: str, *, error: bool = False, busy: bool = False
    ) -> None:
        def update() -> None:
            color = "#a00" if error else "#0a5c0a"
            self.status_label.configure(text=message, foreground=color)
            if busy:
                self._start_progress()
            else:
                self._stop_progress()

        if threading.current_thread() is threading.main_thread():
            update()
        else:
            self.master.after(0, update)

    def _start_progress(self) -> None:
        if not hasattr(self, "progress"):
            return

        def start() -> None:
            try:
                self.progress.configure(mode="indeterminate")
                self.progress.start(12)
            except TclError:
                pass

        if threading.current_thread() is threading.main_thread():
            start()
        else:
            self.master.after(0, start)

    def _stop_progress(self) -> None:
        if not hasattr(self, "progress"):
            return

        def stop() -> None:
            try:
                self.progress.stop()
                self.progress.configure(mode="determinate", value=0, maximum=100)
            except TclError:
                pass

        if threading.current_thread() is threading.main_thread():
            stop()
        else:
            self.master.after(0, stop)

    def _set_busy_ui(self, busy: bool) -> None:
        state = "disabled" if busy else "normal"

        cursor_name = "watch" if sys.platform != "win32" else "wait"

        try:
            self.master.configure(cursor=cursor_name if busy else "")
        except TclError:
            pass

        for widget in self._busy_widgets:
            try:
                widget.configure(state=state)
            except TclError:
                continue

    def _start_background_task(
        self,
        task_id: str,
        worker: Callable[[], Any],
        *,
        status_message: str | None = None,
    ) -> None:
        if self._active_task is not None:
            messagebox.showinfo(
                "Operation in progress",
                "Please wait for the current operation to finish.",
            )
            return

        self._active_task = task_id
        self._set_busy_ui(True)
        if status_message:
            self._set_status(status_message, busy=True)
        else:
            self._set_status("Working...", busy=True)

        thread = threading.Thread(
            target=self._run_background_task,
            args=(task_id, worker),
            daemon=True,
        )
        thread.start()

    def _run_background_task(self, task_id: str, worker: Callable[[], Any]) -> None:
        try:
            result = worker()
        except Exception as exc:  # pragma: no cover - defensive logging
            tb_text = traceback.format_exc()
            self._task_queue.put((task_id, "error", exc, tb_text))
        else:
            self._task_queue.put((task_id, "success", result, None))

    def _poll_async_queue(self) -> None:
        processed = False
        while True:
            try:
                task_id, outcome, payload, detail = self._task_queue.get_nowait()
            except queue.Empty:
                break

            processed = True

            if task_id == "generation":
                if outcome == "success":
                    self._handle_generation_success(payload)
                else:
                    self._handle_generation_error(payload, detail)
            elif task_id == "quick_tres":
                if outcome == "success":
                    self._handle_generation_success(payload)
                else:
                    self._handle_generation_error(payload, detail)
            elif task_id == "builder_help":
                if outcome == "success":
                    self._handle_builder_help_success(payload)
                else:
                    self._handle_builder_help_error(payload, detail)
            elif task_id == "debug_refresh":
                if outcome == "success":
                    self._handle_debug_refresh_success(payload)
                else:
                    self._handle_debug_refresh_error(payload, detail)
                self._debug_refresh_active = False
            elif task_id == "treefile_list":
                if outcome == "success":
                    self._handle_treefile_list_success(payload)
                else:
                    self._handle_treefile_list_error(payload, detail)
            elif task_id == "treefile_extract":
                if outcome == "success":
                    self._handle_treefile_extract_success(payload)
                else:
                    self._handle_treefile_extract_error(payload, detail)

            if task_id == self._active_task:
                self._active_task = None
                self._set_busy_ui(False)

        if not processed and self._active_task is None:
            self._stop_progress()

        self.master.after(150, self._poll_async_queue)

    # ------------------------------------------------------------------
    # Tree file handlers
    # ------------------------------------------------------------------
    def _handle_treefile_list_success(
        self, payload: tuple[Path, list[TreeFileListEntry]]
    ) -> None:
        treefile, entries = payload
        self._populate_treefile_listing(treefile, entries)
        self._set_status(f"Loaded {len(entries)} entries from {treefile.name}.")
        self._log(f"Loaded {len(entries)} entries from {treefile}.\n")

    def _handle_treefile_list_error(
        self, exc: Exception, traceback_text: str | None
    ) -> None:
        if self._maybe_retry_treefile_operation("treefile_list", exc):
            return
        message, detail = self._format_treefile_error(
            exc, traceback_text, context="list tree file"
        )
        messagebox.showerror("Tree file", message)
        self._set_status("Failed to list tree file contents.", error=True)
        self._log(f"Tree file list error: {detail}\n")

    def _handle_treefile_extract_success(
        self, payload: tuple[Path, list[Path]]
    ) -> None:
        treefile, extracted_paths = payload
        self._set_status(
            f"Extracted {len(extracted_paths)} files from {treefile.name}."
        )
        self._log(
            f"Extracted {len(extracted_paths)} files from {treefile}.\n"
        )

    def _handle_treefile_extract_error(
        self, exc: Exception, traceback_text: str | None
    ) -> None:
        if self._maybe_retry_treefile_operation("treefile_extract", exc):
            return
        message, detail = self._format_treefile_error(
            exc, traceback_text, context="extract tree file"
        )
        messagebox.showerror("Tree file extraction", message)
        self._set_status("Failed to extract tree file contents.", error=True)
        self._log(f"Tree file extract error: {detail}\n")

    def _format_treefile_error(
        self, exc: Exception, traceback_text: str | None, *, context: str
    ) -> tuple[str, str]:
        if isinstance(exc, TreeFileExtractorError):
            message = str(exc)
            detail_lines = [message]
            if exc.stdout:
                detail_lines.append(f"Stdout: {exc.stdout.strip()}")
            if exc.stderr:
                detail_lines.append(f"Stderr: {exc.stderr.strip()}")
            return message, "\n".join(detail_lines)
        message = f"Failed to {context}: {exc}"
        detail = f"{message}\n\n{traceback_text}" if traceback_text else message
        return message, detail

    def _normalize_treefile_path(self, treefile: Path) -> Path:
        try:
            return treefile.resolve()
        except RuntimeError:
            return treefile

    def _start_treefile_list_task(self, treefile: Path, passphrase: str) -> None:
        treefile_key = self._normalize_treefile_path(treefile)
        self._treefile_task_context = {
            "task_id": "treefile_list",
            "treefile": treefile_key,
        }

        def worker() -> tuple[Path, list[TreeFileListEntry]]:
            extractor = resolve_treefile_extractor()
            result = list_treefile(
                treefile_key,
                extractor=extractor,
                passphrase=passphrase,
            )
            return treefile_key, result.entries

        self._start_background_task(
            "treefile_list", worker, status_message="Listing tree file contents..."
        )

    def _start_treefile_extract_task(
        self,
        treefile: Path,
        output_dir: Path,
        entries: list[str],
        passphrase: str,
    ) -> None:
        treefile_key = self._normalize_treefile_path(treefile)
        self._treefile_task_context = {
            "task_id": "treefile_extract",
            "treefile": treefile_key,
            "output_dir": output_dir,
            "entries": entries,
        }

        def worker() -> tuple[Path, list[Path]]:
            extractor = resolve_treefile_extractor()
            result = extract_treefile(
                treefile_key,
                output_dir,
                extractor=extractor,
                passphrase=passphrase,
                entries=entries,
            )
            return treefile_key, result.extracted_paths

        self._start_background_task(
            "treefile_extract",
            worker,
            status_message="Extracting selected entries...",
        )

    def _maybe_retry_treefile_operation(self, task_id: str, exc: Exception) -> bool:
        if not isinstance(exc, TreeFileExtractorError):
            return False
        if not self._is_treefile_passphrase_error(exc):
            return False
        context = self._treefile_task_context
        if not context or context.get("task_id") != task_id:
            return False

        treefile: Path = context["treefile"]
        cached = self._treefile_passphrases.get(treefile)
        if cached:
            response = messagebox.askyesnocancel(
                "Tree file passphrase",
                (
                    "The tree file operation failed due to a passphrase error.\n\n"
                    "Select Yes to enter a new passphrase, No to clear the cached passphrase "
                    "and retry without one, or Cancel to stop."
                ),
            )
            if response is None:
                return False
            if response is False:
                self._treefile_passphrases.pop(treefile, None)
                self._queue_treefile_retry(context, passphrase="")
                return True
            passphrase = self._prompt_treefile_passphrase(treefile, allow_cached=False)
            if passphrase is None:
                return False
            self._queue_treefile_retry(context, passphrase=passphrase)
            return True

        response = messagebox.askyesno(
            "Tree file passphrase",
            (
                "The tree file operation failed due to a passphrase error.\n\n"
                "Would you like to retry with a new passphrase?"
            ),
        )
        if not response:
            return False
        passphrase = self._prompt_treefile_passphrase(treefile, allow_cached=False)
        if passphrase is None:
            return False
        self._queue_treefile_retry(context, passphrase=passphrase)
        return True

    def _queue_treefile_retry(
        self, context: dict[str, Any], *, passphrase: str
    ) -> None:
        def retry() -> None:
            task_id = context.get("task_id")
            treefile: Path = context["treefile"]
            if task_id == "treefile_list":
                self._start_treefile_list_task(treefile, passphrase)
                return
            if task_id == "treefile_extract":
                output_dir: Path = context["output_dir"]
                entries: list[str] = context["entries"]
                self._start_treefile_extract_task(
                    treefile,
                    output_dir,
                    entries,
                    passphrase,
                )

        self.master.after(0, retry)

    def _is_treefile_passphrase_error(self, exc: TreeFileExtractorError) -> bool:
        return exc.passphrase_error

    # ------------------------------------------------------------------
    # Debugging helpers
    # ------------------------------------------------------------------
    def _run_builder_help(self) -> None:
        self.log_notebook.select(self.debug_tab)

        def worker() -> tuple[TreeFileBuilder, subprocess.CompletedProcess[str]]:
            builder = self._resolve_builder()
            if builder.using_internal:
                raise TreeFileBuilderError(
                    "Internal TreeFileBuilder in use; no external --help output available."
                )
            completed = subprocess.run(  # nosec B603 - executable path chosen by user
                [str(builder.executable), "--help"],
                capture_output=True,
                text=True,
                check=False,
            )
            return builder, completed

        self._start_background_task(
            "builder_help", worker, status_message="Running TreeFileBuilder --help..."
        )

    def _handle_builder_help_success(
        self, payload: tuple[TreeFileBuilder, subprocess.CompletedProcess[str]]
    ) -> None:
        builder, completed = payload
        output_lines = [
            f"Executable: {builder.executable}",
            f"Command: {builder.executable} --help",
            f"Return code: {completed.returncode}",
            "Stdout:",
            completed.stdout.strip() or "<no output>",
            "Stderr:",
            completed.stderr.strip() or "<no output>",
            "",
        ]
        self._write_debug_output("\n".join(output_lines))
        self._set_status("Captured TreeFileBuilder --help output.")
        self._log("Captured TreeFileBuilder --help output.\n")

    def _handle_builder_help_error(self, exc: Exception, traceback_text: str | None) -> None:
        if isinstance(exc, TreeFileBuilderError):
            message = str(exc)
            messagebox.showerror("TreeFileBuilder", message)
            self._write_debug_output(f"Unable to run --help:\n{message}")
        else:
            message = f"Failed to run --help: {exc}"
            messagebox.showerror("TreeFileBuilder", message)
            detail = f"{message}\n\n{traceback_text}" if traceback_text else message
            self._write_debug_output(detail)
        self._set_status("Failed to capture builder help output.", error=True)
        self._log(f"Debug error: {exc}\n")

    def _refresh_debug_info(self) -> None:
        if self._builder_trace_after is not None:
            callback_id = self._builder_trace_after
            self._builder_trace_after = None
            try:
                self.master.after_cancel(callback_id)
            except Exception:  # pragma: no cover - defensive cancellation
                pass

        if self._debug_refresh_active:
            return

        self._debug_refresh_active = True
        self.debug_builder_path.set("Resolving TreeFileBuilder executable...")
        self.debug_capabilities.set("Detecting capabilities...")

        def worker() -> None:
            try:
                builder = self._resolve_builder()
            except Exception as exc:  # pragma: no cover - defensive logging
                tb_text = traceback.format_exc()
                self._task_queue.put(("debug_refresh", "error", exc, tb_text))
            else:
                self._task_queue.put(("debug_refresh", "success", builder, None))

        threading.Thread(target=worker, daemon=True).start()

    def _handle_debug_refresh_success(self, builder: TreeFileBuilder) -> None:
        if builder.using_internal:
            self.debug_builder_path.set("Internal TreeFileBuilder")
        else:
            self.debug_builder_path.set(str(builder.executable))
        caps = builder.capabilities
        features = [
            ("Passphrase (--passphrase)", caps.supports_passphrase),
            ("Encrypt (--encrypt)", caps.supports_encrypt),
            ("Disable encrypt (--noEncrypt)", caps.supports_no_encrypt),
            ("Quiet (--quiet)", caps.supports_quiet),
            ("Dry run (--noCreate)", caps.supports_dry_run),
        ]
        supported = ", ".join(name for name, flag in features if flag) or "None"
        missing = ", ".join(name for name, flag in features if not flag)
        summary = f"Supports: {supported}."
        if missing:
            summary += f" Missing: {missing}."
        self.debug_capabilities.set(summary)

    def _handle_debug_refresh_error(self, exc: Exception, traceback_text: str | None) -> None:
        if isinstance(exc, TreeFileBuilderError):
            message = str(exc)
        else:
            message = f"Unable to resolve TreeFileBuilder executable: {exc}"
        self.debug_builder_path.set(message)
        self.debug_capabilities.set("Capabilities unavailable.")
        detail = message
        if traceback_text and not isinstance(exc, TreeFileBuilderError):
            detail = f"{message}\n\n{traceback_text}"
        self._write_debug_output(f"Unable to resolve TreeFileBuilder executable:\n{detail}")

    # ------------------------------------------------------------------
    # Core workflow
    # ------------------------------------------------------------------
    def _find_default_executable(self) -> str:
        for candidate in DEFAULT_EXECUTABLE_NAMES:
            if os.path.basename(candidate) == candidate:
                found = shutil.which(candidate)
                if found:
                    return found
            else:
                path = Path(candidate)
                if path.exists():
                    return str(path)
        return ""

    def _handle_generation_success(self, results: list[TreeBuildResult]) -> None:
        if not results:
            self._set_status("Nothing to do.")
            return

        self.log_notebook.select(self.log_tab)

        for result in results:
            self._log(
                "\n".join(
                    [
                        f"Command: {' '.join(result.command)}",
                        f"Return code: {result.returncode}",
                        f"Output file: {result.output}",
                        "Stdout:",
                        result.stdout.strip() or "<no output>",
                        "Stderr:",
                        result.stderr.strip() or "<no output>",
                        "",
                    ]
                )
            )

        self._set_status(self._summarize_generation_results(results))

    def _handle_generation_error(
        self, exc: Exception, traceback_text: str | None
    ) -> None:
        self.log_notebook.select(self.log_tab)

        if isinstance(exc, ValueError):
            title = "Generation failed"
            message = str(exc)
        elif isinstance(exc, (ResponseFileBuilderError, TreeFileBuilderError)):
            title = "Generation failed"
            message = str(exc)
        else:
            title = "Unexpected error"
            message = f"{exc}"

        messagebox.showerror(title, message)

        if isinstance(exc, (ResponseFileBuilderError, TreeFileBuilderError, ValueError)):
            self._log(f"Error: {message}\n")
        else:
            detail = traceback_text or message
            self._log(f"Unexpected error: {message}\n{detail}\n")

        self._set_status("Generation failed", error=True)

    def _generate_tree_files(self) -> None:
        try:
            validated_inputs = self._validate_inputs()
        except ValueError as exc:
            messagebox.showerror("Generation failed", str(exc))
            self._log(f"Error: {exc}\n")
            self._set_status("Generation failed", error=True)
            return

        self.log_notebook.select(self.log_tab)

        def worker() -> list[TreeBuildResult]:
            return self._run_generation(validated_inputs)

        self._start_background_task(
            "generation", worker, status_message="Generating tree files..."
        )

    def _prompt_encrypted_extension(self, *, title: str, prompt: str) -> str | None:
        response = messagebox.askyesnocancel(
            title,
            f"{prompt}\n\nYes: build .tresx\nNo: build .tres",
        )
        if response is None:
            return None
        return ".tresx" if response else ".tres"

    def _normalize_encrypted_output(self, output_path: Path, suffix: str) -> Path:
        if output_path.suffix.lower() not in {".tres", ".tresx"}:
            return output_path.with_suffix(suffix)
        return output_path

    def _prompt_quick_tres_inputs(self) -> tuple[Path, Path, str | None] | None:
        selected = filedialog.askdirectory(title="Select source folder")
        if not selected:
            return None

        source_path = Path(selected)
        if not source_path.exists() or not source_path.is_dir():
            messagebox.showerror(
                "Quick .tres/.tresx", f"Folder not found: {source_path}"
            )
            return None

        extension = self._prompt_encrypted_extension(
            title="Encrypted output format",
            prompt="Choose the encrypted output format.",
        )
        if extension is None:
            return None

        output_path = filedialog.asksaveasfilename(
            title="Save encrypted .tres/.tresx file",
            defaultextension=extension,
            initialdir=str(source_path.parent),
            initialfile=f"{source_path.name}{extension}",
            filetypes=[
                ("Encrypted tree archive (.tres)", "*.tres"),
                ("Encrypted tree archive (.tresx)", "*.tresx"),
                ("All files", "*"),
            ],
        )
        if not output_path:
            return None

        output = self._normalize_encrypted_output(Path(output_path), extension)

        passphrase = simpledialog.askstring(
            "Passphrase",
            "Enter a passphrase to encrypt the .tres/.tresx file (optional).",
            show="*",
        )
        if passphrase is None:
            return None
        return source_path, output, passphrase.strip() or None

    def _quick_create_tres(self) -> None:
        inputs = self._prompt_quick_tres_inputs()
        if inputs is None:
            return
        source_path, output, passphrase = inputs
        output_extension = output.suffix.lower()

        self.log_notebook.select(self.log_tab)
        self._log(f"Quick {output_extension} source: {source_path}\n")
        self._log(f"Quick {output_extension} output: {output}\n")

        def worker() -> list[TreeBuildResult]:
            return self._run_quick_tres(
                sources=[source_path],
                entry_root=source_path,
                output_path=output,
                passphrase=passphrase,
                run_preflight=False,
                use_gpu=False,
            )

        self._start_background_task(
            "quick_tres",
            worker,
            status_message=f"Generating {output_extension} file...",
        )

    def _quick_create_tres_smart(self, *, use_gpu: bool) -> None:
        inputs = self._prompt_quick_tres_inputs()
        if inputs is None:
            return
        source_path, output, passphrase = inputs
        output_extension = output.suffix.lower()

        self.log_notebook.select(self.log_tab)
        self._log(f"Smart {output_extension} source: {source_path}\n")
        self._log(f"Smart {output_extension} output: {output}\n")

        def worker() -> list[TreeBuildResult]:
            return self._run_quick_tres(
                sources=[source_path],
                entry_root=source_path,
                output_path=output,
                passphrase=passphrase,
                run_preflight=True,
                use_gpu=use_gpu,
            )

        self._start_background_task(
            "quick_tres",
            worker,
            status_message=(
                f"Running smart preflight and generating {output_extension}..."
            ),
        )

    def _create_tres_from_current_sources(
        self, *, run_preflight: bool, use_gpu: bool
    ) -> None:
        source_text = self.source_folder.get().strip()
        if not source_text:
            messagebox.showerror(
                "Create .tres/.tresx",
                "Select a primary source folder before creating a .tres/.tresx file.",
            )
            return

        source_path = Path(source_text)
        if not source_path.exists() or not source_path.is_dir():
            messagebox.showerror(
                "Create .tres/.tresx",
                f"Primary source folder not found: {source_path}",
            )
            return

        output_dir_text = self.output_directory.get().strip()
        output_dir = Path(output_dir_text or source_path.parent)
        output_dir.mkdir(parents=True, exist_ok=True)

        base_name = self.base_name.get().strip() or source_path.name
        extension = self._prompt_encrypted_extension(
            title="Encrypted output format",
            prompt="Choose the encrypted output format for the current sources.",
        )
        if extension is None:
            return

        output_path = (output_dir / f"{base_name}{extension}").resolve()

        passphrase = self.passphrase.get().strip() or None

        sources = [source_path] + self.update_paths
        self.log_notebook.select(self.log_tab)
        self._log(
            f"Create {extension} sources: {', '.join(str(path) for path in sources)}\n"
        )
        self._log(f"Create {extension} output: {output_path}\n")

        def worker() -> list[TreeBuildResult]:
            return self._run_quick_tres(
                sources=sources,
                entry_root=source_path,
                output_path=output_path,
                passphrase=passphrase,
                run_preflight=run_preflight,
                use_gpu=use_gpu,
            )

        status = f"Generating {extension} file..."
        if run_preflight:
            status = f"Running smart preflight and generating {extension}..."
        self._start_background_task("quick_tres", worker, status_message=status)

    def _validate_inputs(self) -> tuple[Path, Path, str, bool, bool, bool, str | None]:
        source = Path(self.source_folder.get().strip())
        if not source.exists() or not source.is_dir():
            raise ValueError("Please select a valid source folder.")

        output_dir_text = self.output_directory.get().strip()
        output_dir = Path(output_dir_text or source.parent)
        output_dir.mkdir(parents=True, exist_ok=True)

        base_name = self.base_name.get().strip()
        if not base_name:
            raise ValueError("Please enter a base file name.")

        build_tre = self.build_tre.get()
        build_tres = self.build_tres.get()
        build_tresx = self.build_tresx.get()
        if not build_tre and not build_tres and not build_tresx:
            raise ValueError(
                "Select at least one output format (.tre, .tres, or .tresx)."
            )

        passphrase = self.passphrase.get().strip() or None

        return (
            source,
            output_dir,
            base_name,
            build_tre,
            build_tres,
            build_tresx,
            passphrase,
        )

    def _prepare_response_file(self, source: Path, temp_dir: Path) -> Path:
        sources = [source] + self.update_paths
        builder = ResponseFileBuilder(entry_root=source, allow_overrides=True)
        response_path = temp_dir / "treebuilder.rsp"
        result = builder.write(destination=response_path, sources=sources)
        self._log(
            f"Response file created at {response_path} with {len(result.entries)} entries.\n"
        )
        self._log_tree_size_estimate(result.entries)
        return result.path

    def _log_tree_size_estimate(self, entries: Sequence[ResponseFileEntry]) -> None:
        estimate = estimate_tree_size(entries)
        compressed_pct = estimate.compressed_ratio * 100
        self._log(
            "Estimated payload: "
            f"{estimate.total_files} files, "
            f"{estimate.total_bytes:,} bytes total "
            f"({compressed_pct:.1f}% already compressed).\n"
        )
        if estimate.exceeds_limit:
            limit_bytes = estimate.limit_bytes
            raise ValueError(
                "Tree file exceeds 32-bit offset limits. "
                f"Estimated payload {estimate.total_bytes:,} bytes is over "
                f"{limit_bytes:,} bytes. Split the archive or remove content "
                "to stay under the limit."
            )

    def _log_builder_debug(
        self,
        builder: TreeFileBuilder,
        response_file: Path,
        output_file: Path,
        *,
        no_toc_compression: bool = False,
        no_file_compression: bool = False,
        dry_run: bool = False,
        quiet: bool = False,
        force_encrypt: bool = False,
        disable_encrypt: bool = False,
        passphrase: str | None = None,
    ) -> None:
        capabilities = builder.capabilities
        feature_map = {
            "passphrase": capabilities.supports_passphrase,
            "encrypt": capabilities.supports_encrypt,
            "no-encrypt": capabilities.supports_no_encrypt,
            "quiet": capabilities.supports_quiet,
            "dry-run": capabilities.supports_dry_run,
            "gpu": capabilities.supports_gpu,
        }
        supported = [name for name, enabled in feature_map.items() if enabled]
        unsupported = [name for name, enabled in feature_map.items() if not enabled]

        if builder.using_internal:
            self._log("TreeFileBuilder: internal implementation\n")
        else:
            self._log(f"TreeFileBuilder: {builder.executable}\n")
        if supported:
            self._log(f"Supports: {', '.join(supported)}\n")
        if unsupported:
            self._log(f"Unsupported: {', '.join(unsupported)}\n")

        self._log(f"Response file: {response_file}\n")
        self._log(f"Output file: {output_file}\n")
        self._log(f"Encrypt output: {force_encrypt}\n")
        self._log(f"No TOC compression: {no_toc_compression}\n")
        self._log(f"No file compression: {no_file_compression}\n")
        self._log(f"Dry run: {dry_run}\n")
        self._log(f"Quiet: {quiet}\n")
        self._log(f"Passphrase set: {'yes' if passphrase else 'no'}\n")
        passphrase_preview = "<redacted>" if passphrase else None
        preview_command = builder.build_command(
            response_file=response_file,
            output_file=output_file,
            no_toc_compression=no_toc_compression,
            no_file_compression=no_file_compression,
            dry_run=dry_run,
            quiet=quiet,
            force_encrypt=force_encrypt,
            disable_encrypt=disable_encrypt,
            passphrase=passphrase_preview,
        )
        self._log(f"Command: {' '.join(preview_command)}\n")

    def _resolve_builder(self) -> TreeFileBuilder:
        builder_override = self.builder_path.get().strip() or None
        return TreeFileBuilder(executable=builder_override)

    def _run_generation(
        self,
        inputs: tuple[Path, Path, str, bool, bool, bool, str | None] | None = None,
    ) -> list[TreeBuildResult]:
        if inputs is None:
            inputs = self._validate_inputs()

        (
            source,
            output_dir,
            base_name,
            build_tre,
            build_tres,
            build_tresx,
            passphrase,
        ) = inputs

        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            response_file = self._prepare_response_file(source, temp_path)

            builder = self._resolve_builder()
            if build_tres or build_tresx:
                if not builder.capabilities.supports_encrypt:
                    raise ValueError(
                        "TreeFileBuilder does not support encryption. "
                        "Upgrade the toolchain to build .tres/.tresx files."
                    )
                if passphrase and not builder.capabilities.supports_passphrase:
                    raise ValueError(
                        "TreeFileBuilder does not support passphrases. "
                        "Upgrade the toolchain to build .tres/.tresx files."
                    )
            results = []

            if build_tre:
                output_path = (output_dir / f"{base_name}.tre").resolve()
                self._log_builder_debug(
                    builder,
                    response_file,
                    output_path,
                    disable_encrypt=True,
                )
                self._log(f"\nBuilding {output_path}...\n")
                result = builder.build(
                    response_file=response_file,
                    output_file=output_path,
                    disable_encrypt=True,
                )
                results.append(result)

            if build_tres:
                output_path = (output_dir / f"{base_name}.tres").resolve()
                self._log_builder_debug(
                    builder,
                    response_file,
                    output_path,
                    force_encrypt=True,
                    passphrase=passphrase,
                )
                self._log(f"\nBuilding {output_path}...\n")
                result = builder.build(
                    response_file=response_file,
                    output_file=output_path,
                    force_encrypt=True,
                    passphrase=passphrase or None,
                )
                results.append(result)

            if build_tresx:
                output_path = (output_dir / f"{base_name}.tresx").resolve()
                self._log_builder_debug(
                    builder,
                    response_file,
                    output_path,
                    force_encrypt=True,
                    passphrase=passphrase,
                )
                self._log(f"\nBuilding {output_path}...\n")
                result = builder.build(
                    response_file=response_file,
                    output_file=output_path,
                    force_encrypt=True,
                    passphrase=passphrase or None,
                )
                results.append(result)

            return results

    def _run_quick_tres(
        self,
        sources: list[Path],
        entry_root: Path,
        output_path: Path,
        passphrase: str | None,
        *,
        run_preflight: bool,
        use_gpu: bool,
    ) -> list[TreeBuildResult]:
        if run_preflight:
            self._log("Running smart preflight...\n")
            report = run_tree_preflight(
                sources,
                use_gpu=use_gpu,
                encrypting=True,
                status_callback=lambda message: self._log(f"{message}\n"),
            )
            for line in format_preflight_report(report):
                self._log(f"{line}\n")

        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            response_builder = ResponseFileBuilder(
                entry_root=entry_root, allow_overrides=True
            )
            response_path = temp_path / "treebuilder.rsp"
            response_result = response_builder.write(
                destination=response_path, sources=sources
            )
            self._log(
                f"Response file created at {response_path} with {len(response_result.entries)} entries.\n"
            )
            self._log_tree_size_estimate(response_result.entries)

            builder = self._resolve_builder()
            if not builder.capabilities.supports_encrypt:
                raise ValueError(
                    "TreeFileBuilder does not support encryption. "
                    "Upgrade the toolchain to build .tres/.tresx files."
                )
            if passphrase and not builder.capabilities.supports_passphrase:
                raise ValueError(
                    "TreeFileBuilder does not support passphrases. "
                    "Upgrade the toolchain to build .tres/.tresx files."
                )
            if output_path.suffix.lower() not in {".tres", ".tresx"}:
                raise ValueError("Output file must use a .tres or .tresx extension.")
            self._log_builder_debug(
                builder,
                response_result.path,
                output_path,
                force_encrypt=True,
                passphrase=passphrase,
            )
            self._log(f"\nBuilding {output_path}...\n")
            result = builder.build(
                response_file=response_result.path,
                output_file=output_path,
                force_encrypt=True,
                passphrase=passphrase or None,
            )
            return [result]

    def _summarize_generation_results(self, results: list[TreeBuildResult]) -> str:
        suffixes: list[str] = []
        for result in results:
            suffix = Path(result.output).suffix.lower()
            if suffix and suffix not in suffixes:
                suffixes.append(suffix)

        if not suffixes:
            return "Tree files generated successfully."
        if len(suffixes) == 1:
            return f"{suffixes[0]} file generated successfully."
        if len(suffixes) == 2:
            summary = f"{suffixes[0]} and {suffixes[1]}"
        else:
            summary = ", ".join(suffixes[:-1]) + f", and {suffixes[-1]}"
        return f"{summary} files generated successfully."


def main() -> None:
    try:
        root = Tk()
    except TclError as exc:
        print("Unable to start TreeFileBuilderGui:", exc, file=sys.stderr)
        if sys.platform != "win32" and not os.environ.get("DISPLAY"):
            print(
                "No graphical display detected. Set the DISPLAY environment variable "
                "or run on a system with a GUI.",
                file=sys.stderr,
            )
        sys.exit(1)

    gui = TreeFileBuilderGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
