package ovh.datanet.dataki.client.ui;

/** A single chat message for the RecyclerView. */
public class Message {
    public static final String ROLE_USER = "user";
    public static final String ROLE_ASSISTANT = "assistant";

    public final String role;
    public final StringBuilder content;

    public Message(String role, String content) {
        this.role = role;
        this.content = new StringBuilder(content == null ? "" : content);
    }
}
